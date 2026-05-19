import json
import os
import sqlite3

from datetime import datetime, timedelta, timezone

from flask import Flask, render_template, request
from functools import wraps
from flask_cors import CORS

from functools import lru_cache
from pathlib import Path

app = Flask(__name__)
CORS(app)  # Allow CORS for the backend URL

SQL_DIR = Path("sql")
DB_PATH = "/data/stations.db"


@lru_cache(maxsize=None)
def load_sql(name):
    return (SQL_DIR / name).read_text()

def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    conn = get_db()
    with conn:
        script = load_sql("schema.sql")
        conn.executescript(script)
    
    conn.close()

def load_api_keys():
    with open("api_keys.json") as f:
        keys = json.load(f)
        valid_keys = {key: True for key in keys}
    return valid_keys
    
def require_api_key(func):
    @wraps(func)
    def wrapper(*args, **kwargs):
        api_key = request.headers.get("Authorization")
        if not api_key:
            return {"error": "API key is missing"}, 401

        if not api_key in valid_keys:
            return {"error": "Invalid API key"}, 403

        return func(*args, **kwargs)
    return wrapper

with app.app_context():
    valid_keys = load_api_keys()
    init_db()
    
## API ##

@app.route("/api/space", methods=["POST"])
def get_space_state():
    data = request.json
    station_id = data.get("station")
    space_id = data.get("space")

    conn = get_db()
    with conn:
        query = load_sql("get_space_state.sql")
        row = conn.execute(query, {"id": space_id, "station": station_id}).fetchone()
    conn.close()

    if not row:
        return {"error": "Space not found"}, 404
    
    return row  

@app.route("/api/reset_db", methods=["POST"])
@require_api_key
def reset_db():
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)
    init_db()
    return {"status": "ok"}, 200

@app.route("/api/get_table", methods=["POST"])
@require_api_key
def get_table():
    data = request.json
    table = data.get("table")
    if table not in ["stations", "spaces", "events"]:
        return {"error": "Invalid table name"}, 400

    conn = get_db()
    with conn:
        query = f"SELECT * FROM {table}"
        rows = conn.execute(query).fetchall()
    
    conn.close()

    return {
        "data": [dict(row) for row in rows]
    }

@app.route("/api/overview")
def get_overview():
    conn = get_db()
    with conn:

        query = load_sql("overview.sql")
        rows = conn.execute(query).fetchall()
    
    conn.close()

    return {
        "data": [dict(row) for row in rows]
    }

@app.route("/api/stations")
def get_stations():
    conn = get_db()
    with conn:
        cutoff = (datetime.now(timezone.utc) - timedelta(minutes=5)).isoformat()
        query = load_sql("get_stations.sql")
        rows = conn.execute(query, {"cutoff": cutoff}).fetchall()

    conn.close()

    return {
        "data": [dict(row) for row in rows]
    } 

@app.route("/api/create_station", methods=["POST"])
@require_api_key
def create_station():
    data = request.json

    conn = get_db()
    with conn:

        # check if station with the same id already exists
        existing = conn.execute("SELECT * FROM stations WHERE id = ?", (data["id"],)).fetchone()
    
    if existing:
        conn.close()
        return {"error": "Station with this ID already exists"}, 400
    else:
        with conn:
            query = load_sql("create_station.sql")

            conn.execute(query, {
                "id": data["id"],
                "name": data["name"],
                "lat": data["lat"],
                "lon": data["lon"],
                "keepalive": datetime.now(timezone.utc).isoformat(),
            })

    conn.close()
    return {"status": "ok"}, 201

@app.route("/api/create_space", methods=["POST"])
@require_api_key
def create_space():
    data = request.json

    space_state = get_space_state(data["station"], data["space"])
    if space_state and space_state["state"] != "deleted":
        return {"error": "Space already exists"}, 400

    conn = get_db()
    with conn:
        if space_state and space_state["state"] == "deleted":
            query = load_sql("recreate_space.sql")
            conn.execute(query, {
                "id": data["space"],
                "station": data["station"],
            })
        else:
            query = load_sql("create_space.sql")
            conn.execute(query, {
                "id": data["space"],
                "station": data["station"],
                "state": "free"
            })

        query = load_sql("log_event.sql")
        conn.execute(query, {
            "station": data["station"],
            "space": data["space"],
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "type": "create",
            "rfid": None,
        })
    
    conn.close()

    return {"status": "ok"}, 201

@app.route("/api/delete_space", methods=["POST"])
@require_api_key
def delete_space():
    data = request.json

    space_state = get_space_state(data["station"], data["space"])
    if not space_state:
        return {"error": "Space not found"}, 404
    elif space_state["state"] == "deleted":
        return {"error": "Space is already deleted"}, 400

    conn = get_db()
    with conn:
        query = load_sql("set_space_state.sql")
        conn.execute(query, {
            "id": data["space"],
            "station": data["station"],
            "state": "deleted"
        })

        query = load_sql("log_event.sql")
        conn.execute(query, {
            "station": data["station"],
            "space": data["space"],
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "type": "delete",
            "rfid": None,
        })
    
    conn.close()
    return {"status": "ok"}, 200

@app.route("/api/keep_alive", methods=["POST"])
@require_api_key
def keep_alive():
    data = request.json

    conn = get_db()
    with conn:
        query = load_sql("keepalive.sql")
        conn.execute(query, {
            "id": data["id"],
            "keepalive": datetime.now(timezone.utc).isoformat(),
        })
    
    conn.close()
    return {"status": "ok"}, 200

@app.route("/api/unlock", methods=["POST"])
@require_api_key
def unlock():
    data = request.json

    space_state = get_space_state(data["station"], data["space"])
    if not space_state:
        return {"error": "Space not found"}, 404
    elif space_state["state"] == "free":
        return {"error": "Space is not used"}, 400
    elif space_state["state"] == "deleted":
        return {"error": "Space is deleted"}, 400
    elif space_state["rfid"] != data["rfid"]:
        return {"error": "RFID does not match"}, 403

    conn = get_db()
    with conn:
        query = load_sql("set_space_state.sql")
        conn.execute(query, {
            "id": data["space"],
            "station": data["station"],
            "state": "free"
        })

        query = load_sql("log_event.sql")
        conn.execute(query, {
            "station": data["station"],
            "space": data["space"],
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "type": "unlock",
            "rfid": data["rfid"],
        })
        
    conn.close()
    return {"status": "ok"}, 200

@app.route("/api/lock", methods=["POST"])
@require_api_key
def lock():
    data = request.json

    space_state = get_space_state(data["station"], data["space"])
    if not space_state:
        return {"error": "Space not found"}, 404
    elif space_state["state"] == "used":
        return {"error": "Space is in use"}, 400
    elif space_state["state"] == "deleted":
        return {"error": "Space is deleted"}, 400

    conn = get_db()
    with conn:
        query = load_sql("set_space_state.sql")
        conn.execute(query, {
            "id": data["space"],
            "station": data["station"],
            "state": "used",
            "rfid": data["rfid"],
        })

        query = load_sql("log_event.sql")
        conn.execute(query, {
            "station": data["station"],
            "space": data["space"],
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "type": "lock",
            "rfid": data["rfid"],
        })

    conn.close()
    return {"status": "ok"}, 200