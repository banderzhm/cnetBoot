-- 存档正文与元数据同行保存，从根源上消除文件/数据库双写与补偿。
ALTER TABLE chart_archive_index
    ADD COLUMN payload LONGTEXT NOT NULL AFTER size_bytes;
