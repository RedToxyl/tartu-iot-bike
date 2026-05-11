import json
import os
from datetime import datetime

from flask import Flask, render_template, request
from functools import wraps
from flask_cors import CORS

import sqlite3

app = Flask(__name__)
CORS(app)  # Allow CORS for the backend URL

DB_PATH = "/data/stations.db"

def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def require_api_key(func):
    @wraps(func)
    def wrapper(*args, **kwargs):
        api_key = request.headers.get("Authorization")
        if not api_key:
            return {"error": "API key is missing"}, 401

        with open("api_keys.json") as f:
            valid_keys = json.load(f)

        if not valid_keys.get(api_key):
            return {"error": "Invalid API key"}, 403

        return func(*args, **kwargs)
    return wrapper

@app.route("/api/register", methods=["POST"])
@require_api_key
def register_station():
    data = request.json

    conn = sqlite3.connect(DB_PATH)

    # check if station with the same id already exists
    existing = conn.execute("SELECT * FROM stations WHERE id = ?", (data["id"],)).fetchone()
    if existing:
        conn.close()
        return {"error": "Station with this ID already exists"}, 400
        
    conn.execute(
        "INSERT INTO stations (id, name, longitude, latitude) VALUES (?, ?, ?, ?)",
        (data["id"], data["name"], data["longitude"], data["latitude"])
    )
    conn.commit()
    conn.close()

    return {"status": "ok"}

@app.route("/api/log", methods=["POST"])
@require_api_key
def add_station_data():
    data = request.json

    conn = sqlite3.connect(DB_PATH)
    conn.execute(
        "INSERT INTO measurements (station_id, timestamp, free_spaces, used_spaces, total_spaces) VALUES (?, ?, ?, ?, ?)",
        (data["station_id"], data["timestamp"], data["free_spaces"], data["used_spaces"], data["total_spaces"])
    )
    conn.commit()
    conn.close()

    return {"status": "ok"}

@app.route("/api/stations")
def get_stations():
    conn = get_db()

    rows = conn.execute("SELECT * FROM stations").fetchall()

    conn.close()

    return {
        "data": [dict(row) for row in rows]
    }


@app.route("/api/measurements")
def get_measurements():
    conn = get_db()

    rows = conn.execute("""
        SELECT * FROM measurements 
        WHERE (station_id, timestamp) IN (
            SELECT station_id, MAX(timestamp)
            FROM measurements
            GROUP BY station_id
        )
    """).fetchall()

    conn.close()

    return {
        "data": [dict(row) for row in rows]
    }

@app.route("/api/reset_db", methods=["POST"])
@require_api_key
def reset_db():
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)
    init_db()
    return {"status": "ok"}

def init_db():
    conn = sqlite3.connect(DB_PATH)

    with open("schema.sql") as f:
        conn.executescript(f.read())

    conn.close()

with app.app_context():
    init_db()