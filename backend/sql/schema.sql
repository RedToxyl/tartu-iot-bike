-- Stations table
CREATE TABLE IF NOT EXISTS stations (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    latitude REAL NOT NULL,
    longitude REAL NOT NULL,
    keepalive TEXT NOT NULL
);

-- Spaces table
CREATE TABLE IF NOT EXISTS spaces (
    id TEXT NOT NULL,
    station TEXT NOT NULL,
    state TEXT NOT NULL CHECK (state IN ('free', 'used', 'deleted')),
    PRIMARY KEY (id, station),
    FOREIGN KEY (station) REFERENCES stations(id)
);

-- Events table
CREATE TABLE IF NOT EXISTS events (
    id INTEGER PRIMARY KEY,
    station TEXT NOT NULL,
    space TEXT NOT NULL,
    timestamp TEXT NOT NULL,
    type TEXT NOT NULL CHECK (type IN ('create', 'lock', 'unlock', 'delete')),
    rfid TEXT,
    FOREIGN KEY (station) REFERENCES stations(id),
    FOREIGN KEY (space) REFERENCES spaces(id)
);

CREATE INDEX IF NOT EXISTS idx_events_space_station_time
ON events(space, station, timestamp DESC);