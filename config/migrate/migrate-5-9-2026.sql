CREATE TABLE IF NOT EXISTS project_likes (
    id CHAR(36) NOT NULL,
    project_id CHAR(36) NOT NULL,
    user_id CHAR(36) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uq_project_likes (user_id, project_id),

    CONSTRAINT fk_likes_project
        FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE,
    CONSTRAINT fk_likes_user
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


CREATE TABLE IF NOT EXISTS project_views (
    id CHAR(36) NOT NULL,
    project_id CHAR(36) NOT NULL,
    user_id CHAR(36) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uq_project_views (user_id, project_id),

    CONSTRAINT fk_views_project
        FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE,
    CONSTRAINT fk_views_user
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


CREATE TABLE IF NOT EXISTS project_saved (
    id CHAR(36) NOT NULL,
    project_id CHAR(36) NOT NULL,
    user_id CHAR(36) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uq_project_saved (user_id, project_id),

    CONSTRAINT fk_saved_project
        FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE,
    CONSTRAINT fk_saved_user
        FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELIMITER //

-- 1. Trigger untuk mengurangi likes
CREATE TRIGGER trigger_after_delete_like
AFTER DELETE ON project_likes
FOR EACH ROW
BEGIN
    UPDATE projects SET likes = likes - 1 WHERE id = OLD.project_id AND likes > 0;
END; //

-- 2. Trigger untuk mengurangi saves
CREATE TRIGGER trigger_after_delete_save
AFTER DELETE ON project_saved
FOR EACH ROW
BEGIN
    UPDATE projects SET saves = saves - 1 WHERE id = OLD.project_id AND saves > 0;
END; //

-- 3. Trigger untuk mengurangi views
CREATE TRIGGER trigger_after_delete_view
AFTER DELETE ON project_views
FOR EACH ROW
BEGIN
    UPDATE projects SET views = views - 1 WHERE id = OLD.project_id AND views > 0;
END; //

DELIMITER ;
