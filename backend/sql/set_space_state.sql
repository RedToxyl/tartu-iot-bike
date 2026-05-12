UPDATE spaces 
SET state = :state
WHERE id = :id AND station = :station;