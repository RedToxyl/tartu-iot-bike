SELECT s.state, e.rfid
FROM spaces s
JOIN events e ON s.id = e.space AND s.station = e.station
WHERE s.id = :id AND s.station = :station
ORDER BY e.timestamp DESC
LIMIT 1;