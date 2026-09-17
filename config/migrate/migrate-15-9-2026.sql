CREATE TABLE IF NOT EXISTS upgrade_logs (
    id CHAR(36) NOT NULL,
    user_id CHAR(36) NOT NULL,
    status ENUM('pending', 'approved', 'rejected', 'paid') NOT NULL DEFAULT 'pending',
    rejection_reason VARCHAR(255) NOT NULL DEFAULT '',
    requested_at DATETIME(6) NOT NULL,
    reviewed_at DATETIME(6) NULL,

    PRIMARY KEY (id),

    CONSTRAINT fk_upgrade_user_log
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci;