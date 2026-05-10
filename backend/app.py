import json
from datetime import datetime

from flask import Flask, render_template, request
from functools import wraps
from flask_cors import CORS

import sqlite3

app = Flask(__name__)
CORS(app)  # Allow CORS for the backend URL

DB_PATH = "/data/stations.db"

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
def register_sensor():
    data = request.json

    conn = sqlite3.connect(DB_PATH)
    conn.execute(
        "INSERT INTO stations (id, name, longitude, latitude) VALUES (?, ?, ?, ?)",
        (data["id"], data["name"], data["longitude"], data["latitude"])
    )
    conn.commit()
    conn.close()

    return {"status": "ok"}

@app.route("/api/log", methods=["POST"])
@require_api_key
def add_sensor_data():
    data = request.json

    conn = sqlite3.connect(DB_PATH)
    conn.execute(
        "INSERT INTO measurements (station_id, timestamp, free_spaces, used_spaces, total_spaces) VALUES (?, ?, ?, ?, ?)",
        (data["station_id"], data["time"], data["free_spaces"], data["used_spaces"], data["total_spaces"])
    )
    conn.commit()
    conn.close()

    return {"status": "ok"}

@app.route("/api/stations")
def get_stations():
    conn = sqlite3.connect(DB_PATH)
    rows = conn.execute("SELECT * FROM stations").fetchall()
    conn.close()

    return {"data": rows}

@app.route("/api/measurements")
def get_measurements():
    conn = sqlite3.connect(DB_PATH)
    rows = conn.execute("SELECT * FROM measurements").fetchall()
    conn.close()

    return {"data": rows}

def init_db():
    conn = sqlite3.connect(DB_PATH)

    with open("schema.sql") as f:
        conn.executescript(f.read())

    conn.close()

with app.app_context():
    init_db()