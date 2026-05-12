UPDATE spaces 
SET state = 'free' 
WHERE id = :id AND station = :station;