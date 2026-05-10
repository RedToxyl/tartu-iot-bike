CREATE TABLE IF NOT EXISTS stations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    latitude REAL NOT NULL,
    longitude REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS measurements (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL,
    timestamp DATETIME NOT NULL,
    free_spaces INTEGER NOT NULL,
    used_spaces INTEGER NOT NULL,
    total_spaces INTEGER NOT NULL,
    FOREIGN KEY (station_id) REFERENCES stations(id)
);