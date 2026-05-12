UPDATE stations 
SET keepalive = :keepalive 
WHERE id = :id;