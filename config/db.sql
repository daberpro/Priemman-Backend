-- MariaDB schema for Priemman backend
-- Based on common.proto, user.proto, auth.proto and project.proto
-- MariaDB 10.6+

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

CREATE DATABASE IF NOT EXISTS db_priemman
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE db_priemman;

-- ============================================================
-- USERS / ACCOUNT
-- ============================================================

CREATE TABLE IF NOT EXISTS users (
    id CHAR(36) NOT NULL,
    email VARCHAR(320) NOT NULL,
    first_name VARCHAR(100) NOT NULL DEFAULT '',
    last_name VARCHAR(100) NOT NULL DEFAULT '',
    headline VARCHAR(255) NOT NULL DEFAULT '',
    company VARCHAR(255) NOT NULL DEFAULT '',
    country VARCHAR(100) NOT NULL DEFAULT '',
    city VARCHAR(100) NOT NULL DEFAULT '',
    website_url TEXT NOT NULL,
    avatar_url TEXT NOT NULL,
    is_onboarded BOOLEAN NOT NULL DEFAULT FALSE,
    about_title VARCHAR(255) NOT NULL DEFAULT '',
    about_description TEXT NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),
    UNIQUE KEY uq_users_email (email),
    KEY idx_users_email_lower (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================
-- ACCOUNT
-- ============================================================

CREATE TABLE IF NOT EXISTS work_experiences (
    id CHAR(36) NOT NULL,
    user_id CHAR(36) NOT NULL,
    title VARCHAR(255) NOT NULL DEFAULT '',
    company VARCHAR(255) NOT NULL DEFAULT '',
    is_current BOOLEAN NOT NULL DEFAULT FALSE,
    start_date DATETIME(6) NULL,
    end_date DATETIME(6) NULL,
    description TEXT NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),
    KEY idx_work_experiences_user_id (user_id),

    CONSTRAINT fk_work_experiences_user
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS connected_accounts (
    id CHAR(36) NOT NULL,
    user_id CHAR(36) NOT NULL,
    platform ENUM('INSTAGRAM', 'LINKEDIN', 'GITHUB') NOT NULL,
    handle_or_url TEXT NOT NULL,
    verified BOOLEAN NOT NULL DEFAULT FALSE,
    connected_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),
    UNIQUE KEY uq_connected_account_platform (user_id, platform),
    KEY idx_connected_accounts_user_id (user_id),

    CONSTRAINT fk_connected_accounts_user
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================
-- AUTHENTICATION
-- ============================================================

-- OAuth identity is deliberately separated from users.
CREATE TABLE IF NOT EXISTS oauth_accounts (
    id CHAR(36) NOT NULL,
    user_id CHAR(36) NOT NULL,
    provider ENUM('GOOGLE', 'GITHUB') NOT NULL,
    provider_user_id VARCHAR(255) NOT NULL,
    provider_email VARCHAR(320) NOT NULL DEFAULT '',
    email_verified BOOLEAN NOT NULL DEFAULT FALSE,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),
    UNIQUE KEY uq_oauth_provider_identity (provider, provider_user_id),
    KEY idx_oauth_accounts_user_id (user_id),
    KEY idx_oauth_accounts_email (provider_email),

    CONSTRAINT fk_oauth_accounts_user
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS oauth_states (
    id CHAR(36) NOT NULL,
    state_hash VARCHAR(255) NOT NULL,
    provider ENUM('GOOGLE', 'GITHUB') NOT NULL,
    redirect_uri TEXT NOT NULL,
    expires_at DATETIME(6) NOT NULL,
    consumed_at DATETIME(6) NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),
    UNIQUE KEY uq_oauth_states_hash (state_hash),
    KEY idx_oauth_states_expires_at (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Only the hash of the OTP should be stored.
CREATE TABLE IF NOT EXISTS otp_challenges (
    id CHAR(36) NOT NULL,
    email VARCHAR(320) NOT NULL,
    otp_hash VARCHAR(255) NOT NULL,
    expires_at DATETIME(6) NOT NULL,
    consumed_at DATETIME(6) NULL,
    attempts INT UNSIGNED NOT NULL DEFAULT 0,
    last_sent_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),
    KEY idx_otp_email_created (email, created_at),
    KEY idx_otp_expires_at (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Store only a hash of the bearer session token.
CREATE TABLE IF NOT EXISTS sessions (
    id CHAR(36) NOT NULL,
    token_hash VARCHAR(255) NOT NULL,
    user_id CHAR(36) NOT NULL,
    expires_at DATETIME(6) NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    revoked_at DATETIME(6) NULL,

    PRIMARY KEY (id),
    UNIQUE KEY uq_sessions_token_hash (token_hash),
    KEY idx_sessions_user_id (user_id),
    KEY idx_sessions_expires_at (expires_at),

    CONSTRAINT fk_sessions_user
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- OAuth CSRF state.
CREATE TABLE IF NOT EXISTS oauth_states (
    id CHAR(36) NOT NULL,
    state_hash VARCHAR(255) NOT NULL,
    provider ENUM('GOOGLE', 'GITHUB') NOT NULL,
    redirect_uri TEXT NOT NULL,
    expires_at DATETIME(6) NOT NULL,
    consumed_at DATETIME(6) NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),
    UNIQUE KEY uq_oauth_states_hash (state_hash),
    KEY idx_oauth_states_expires_at (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================
-- PROJECTS
-- ============================================================

CREATE TABLE IF NOT EXISTS projects (
    id CHAR(36) NOT NULL,
    owner_id CHAR(36) NOT NULL,
    title VARCHAR(255) NOT NULL DEFAULT '',
    slug VARCHAR(255) NOT NULL,
    description TEXT NOT NULL,
    cover_media_id CHAR(36) NULL,
    visibility ENUM('PUBLIC', 'UNLISTED', 'DRAFT') NOT NULL DEFAULT 'DRAFT',
    status ENUM('DRAFT', 'PUBLISHED', 'ARCHIVED') NOT NULL DEFAULT 'DRAFT',
    views BIGINT UNSIGNED NOT NULL DEFAULT 0,
    likes BIGINT UNSIGNED NOT NULL DEFAULT 0,
    saves BIGINT UNSIGNED NOT NULL DEFAULT 0,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6),
    published_at DATETIME(6) NULL,

    PRIMARY KEY (id),
    UNIQUE KEY uq_projects_owner_slug (owner_id, slug),
    KEY idx_projects_owner_id (owner_id),
    KEY idx_projects_owner_status (owner_id, status),
    KEY idx_projects_status (status),
    KEY idx_projects_published_at (published_at),

    CONSTRAINT fk_projects_owner
        FOREIGN KEY (owner_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS project_tools (
    project_id CHAR(36) NOT NULL,
    tool VARCHAR(255) NOT NULL,
    sort_order INT UNSIGNED NOT NULL DEFAULT 0,

    PRIMARY KEY (project_id, tool),

    CONSTRAINT fk_project_tools_project
        FOREIGN KEY (project_id)
        REFERENCES projects(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS project_disciplines (
    project_id CHAR(36) NOT NULL,
    discipline VARCHAR(255) NOT NULL,
    sort_order INT UNSIGNED NOT NULL DEFAULT 0,

    PRIMARY KEY (project_id, discipline),

    CONSTRAINT fk_project_disciplines_project
        FOREIGN KEY (project_id)
        REFERENCES projects(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS project_tags (
    project_id CHAR(36) NOT NULL,
    tag VARCHAR(255) NOT NULL,
    sort_order INT UNSIGNED NOT NULL DEFAULT 0,

    PRIMARY KEY (project_id, tag),

    CONSTRAINT fk_project_tags_project
        FOREIGN KEY (project_id)
        REFERENCES projects(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS project_media (
    id CHAR(36) NOT NULL,
    project_id CHAR(36) NOT NULL,
    url TEXT NOT NULL,
    media_type ENUM('IMAGE', 'VIDEO') NOT NULL,
    sort_order INT UNSIGNED NOT NULL DEFAULT 0,
    cloudinary_public_id VARCHAR(255) NOT NULL DEFAULT '',

    PRIMARY KEY (id),
    KEY idx_project_media_project_order (project_id, sort_order),
    KEY idx_project_media_public_id (cloudinary_public_id),

    CONSTRAINT fk_project_media_project
        FOREIGN KEY (project_id)
        REFERENCES projects(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Sinkronisasi untuk DB lama yang sudah punya project_media tanpa kolom public id
SET @col_exists := (
    SELECT COUNT(*)
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'project_media'
      AND COLUMN_NAME = 'cloudinary_public_id'
);

SET @sql := IF(
    @col_exists = 0,
    'ALTER TABLE project_media ADD COLUMN cloudinary_public_id VARCHAR(255) NOT NULL DEFAULT '''', ADD KEY idx_project_media_public_id (cloudinary_public_id)',
    'SELECT 1'
);

PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @fk_exists := (
    SELECT COUNT(*)
    FROM information_schema.TABLE_CONSTRAINTS
    WHERE CONSTRAINT_SCHEMA = DATABASE()
      AND TABLE_NAME = 'projects'
      AND CONSTRAINT_NAME = 'fk_projects_cover_media'
      AND CONSTRAINT_TYPE = 'FOREIGN KEY'
);

SET @sql := IF(
    @fk_exists = 0,
    'ALTER TABLE projects ADD CONSTRAINT fk_projects_cover_media FOREIGN KEY (cover_media_id) REFERENCES project_media(id) ON DELETE SET NULL',
    'SELECT 1'
);

PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

CREATE TABLE IF NOT EXISTS project_collaborators (
    project_id CHAR(36) NOT NULL,
    user_id CHAR(36) NOT NULL,
    role VARCHAR(255) NOT NULL DEFAULT '',
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),

    PRIMARY KEY (project_id, user_id),
    KEY idx_project_collaborators_user_id (user_id),

    CONSTRAINT fk_project_collaborators_project
        FOREIGN KEY (project_id)
        REFERENCES projects(id)
        ON DELETE CASCADE,

    CONSTRAINT fk_project_collaborators_user
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================
-- COLLECTIONS
-- ============================================================

CREATE TABLE IF NOT EXISTS collections (
    id CHAR(36) NOT NULL,
    owner_id CHAR(36) NOT NULL,
    title VARCHAR(255) NOT NULL DEFAULT '',
    description TEXT NOT NULL,
    visibility ENUM('PUBLIC', 'PRIVATE') NOT NULL DEFAULT 'PRIVATE',
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6),

    PRIMARY KEY (id),
    KEY idx_collections_owner_id (owner_id),

    CONSTRAINT fk_collections_owner
        FOREIGN KEY (owner_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS collection_projects (
    collection_id CHAR(36) NOT NULL,
    project_id CHAR(36) NOT NULL,
    sort_order INT UNSIGNED NOT NULL DEFAULT 0,

    PRIMARY KEY (collection_id, project_id),
    KEY idx_collection_projects_project_id (project_id),

    CONSTRAINT fk_collection_projects_collection
        FOREIGN KEY (collection_id)
        REFERENCES collections(id)
        ON DELETE CASCADE,

    CONSTRAINT fk_collection_projects_project
        FOREIGN KEY (project_id)
        REFERENCES projects(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================================
-- MEDIA UPLOADS (tracking aset Cloudinary untuk orphan cleanup)
-- ============================================================

-- status:
--   'orphan'  = sudah diupload tapi belum terikat ke project
--               (akan dihapus sweeper setelah melewati TTL)
--   'in_use'  = sedang dipakai oleh satu/lebih project
CREATE TABLE IF NOT EXISTS media_uploads (
    public_id VARCHAR(255) NOT NULL,
    user_id CHAR(36) NOT NULL,
    resource_type VARCHAR(20) NOT NULL DEFAULT 'image',
    status ENUM('orphan', 'in_use') NOT NULL DEFAULT 'orphan',
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    attached_at DATETIME(6) NULL,

    PRIMARY KEY (public_id),
    KEY idx_media_uploads_user_id (user_id),
    KEY idx_media_uploads_sweep (status, created_at),

    CONSTRAINT fk_media_uploads_user
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

SET FOREIGN_KEY_CHECKS = 1;
