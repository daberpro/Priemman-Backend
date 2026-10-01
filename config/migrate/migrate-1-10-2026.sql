CREATE TABLE IF NOT EXISTS projects_meta_data (
    id CHAR(36) NOT NULL,
    project_id CHAR(36) NOT NULL,
    views BIGINT UNSIGNED NOT NULL DEFAULT 0,
    likes BIGINT UNSIGNED NOT NULL DEFAULT 0,
    saves BIGINT UNSIGNED NOT NULL DEFAULT 0,
    
    PRIMARY KEY (id),
    UNIQUE KEY uq_project_meta_info (project_id),

    CONSTRAINT fk_project_meta_info
        FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

