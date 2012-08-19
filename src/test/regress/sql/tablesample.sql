--
-- TABLESAMPLE TEST
--

CREATE TABLE sample (id INT, name text);

INSERT INTO sample VALUES (1, 'victor');
INSERT INTO sample VALUES (2, 'peter');
INSERT INTO sample VALUES (3, 'james');
INSERT INTO sample VALUES (4, 'paul');
INSERT INTO sample VALUES (5, 'kate');
INSERT INTO sample VALUES (6, 'jane');
INSERT INTO sample VALUES (7, 'samual');
INSERT INTO sample VALUES (8, 'david');
INSERT INTO sample VALUES (9, 'job');
INSERT INTO sample VALUES (10, 'neil');
INSERT INTO sample VALUES (11, 'victor');
INSERT INTO sample VALUES (12, 'peter');
INSERT INTO sample VALUES (13, 'james');
INSERT INTO sample VALUES (14, 'paul');
INSERT INTO sample VALUES (15, 'kate');
INSERT INTO sample VALUES (16, 'jane');
INSERT INTO sample VALUES (17, 'samual');
INSERT INTO sample VALUES (18, 'david');
INSERT INTO sample VALUES (19, 'job');
INSERT INTO sample VALUES (20, 'neil');

CREATE TABLE course (id INT, course text);

INSERT INTO course VALUES (1, 'Physics');
INSERT INTO course VALUES (2, 'Computing');
INSERT INTO course VALUES (2, 'Math');
INSERT INTO course VALUES (3, 'Quantum Mechanics');
INSERT INTO course VALUES (5, 'Life Science');
INSERT INTO course VALUES (6, 'Food Science');
INSERT INTO course VALUES (1, 'Math');
INSERT INTO course VALUES (7, 'Music');
INSERT INTO course VALUES (3, 'Social Science');
INSERT INTO course VALUES (8, 'Material Science');
INSERT INTO course VALUES (9, 'Computing');
INSERT INTO course VALUES (10, 'Computing');
INSERT INTO course VALUES (12, 'Physics');
INSERT INTO course VALUES (13, 'Social Science');
INSERT INTO course VALUES (14, 'Physics');
INSERT INTO course VALUES (15, 'Music');
INSERT INTO course VALUES (16, 'Math');
INSERT INTO course VALUES (17, 'Chemistry');
INSERT INTO course VALUES (14, 'Math');
INSERT INTO course VALUES (18, 'Math');
INSERT INTO course VALUES (20, 'Computing');

ANALYZE sample;
ANALYZE course;

SELECT * FROM sample TABLESAMPLE SYSTEM (60) REPEATABLE (10);

SELECT * FROM sample TABLESAMPLE BERNOULLI (70) REPEATABLE (11);

SELECT * FROM sample TABLESAMPLE BERNOULLI (70) REPEATABLE (11);

SELECT * FROM sample TABLESAMPLE BERNOULLI (70) REPEATABLE (19);

SELECT * FROM sample TABLESAMPLE BERNOULLI (7) REPEATABLE (19);

SELECT * FROM sample TABLESAMPLE BERNOULLI (1) REPEATABLE (19);

SELECT * FROM sample TABLESAMPLE SYSTEM (65) REPEATABLE (20);

SELECT * FROM sample TABLESAMPLE SYSTEM (65) REPEATABLE (2);

SELECT * FROM sample TABLESAMPLE SYSTEM (65) REPEATABLE (20);

SELECT * FROM sample TABLESAMPLE BERNOULLI (70) REPEATABLE (11) order by id limit 5;

SELECT * FROM 
	(SELECT * FROM sample TABLESAMPLE BERNOULLI (70) REPEATABLE (10)) s,
    course c
WHERE s.id = c.id;

SELECT * FROM 
	(SELECT * FROM sample TABLESAMPLE BERNOULLI (60) REPEATABLE (5)) s,
	(SELECT * FROM course TABLESAMPLE SYSTEM (50) REPEATABLE (11)) c
WHERE s.id = c.id;

SELECT * INTO tmp FROM sample TABLESAMPLE BERNOULLI (4);

DROP TABLE tmp;

SELECT * INTO tmp FROM sample TABLESAMPLE SYSTEM (20);

DROP TABLE tmp;

SELECT * INTO tmp FROM
	(SELECT s.id, s.name, c.course FROM
			(SELECT * FROM sample TABLESAMPLE BERNOULLI (60)) s,
			(SELECT * FROM course TABLESAMPLE SYSTEM (50)) c
	 WHERE s.id = c.id) t;

DROP TABLE tmp;

DROP TABLE sample;
DROP TABLE course;
