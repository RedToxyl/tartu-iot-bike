SELECT *
FROM STATIONS
WHERE keepalive > :cutoff;