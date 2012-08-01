DROP TABLE IF EXISTS offline_msg.offline_msg;

CREATE TABLE IF NOT EXISTS offline_msg.offline_msg (
	from_uin INT UNSIGNED NOT NULL,
	to_uin INT UNSIGNED NOT NULL,
	type TINYINT UNSIGNED NOT NULL,
	message VARCHAR(4096) NULL,
	rsv_1 VARCHAR(32) NULL,
	rsv_2 VARCHAR(32) NULL,
	rsv_3 VARCHAR(32) NULL,
	rsv_4 VARCHAR(32) NULL,
	PRIMARY KEY (from_uin, to_uin) )
ENGINE = InnoDB;

GRANT ALL ON offline_msg.offline_msg TO root;
GRANT ALL ON offline_msg.offline_msg TO im_root;
GRANT SELECT, UPDATE, INSERT, DELETE ON offline_msg.offline_msg TO im_user;
