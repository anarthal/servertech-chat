--
-- Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
--
-- Distributed under the Boost Software License, Version 1.0. (See accompanying
-- file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
--

-- DB setup code. This is executed once at startup.
-- This should be improved when implementing
-- https://github.com/anarthal/servertech-chat/issues/11

-- Database
CREATE DATABASE IF NOT EXISTS servertech_chat;
USE servertech_chat;

-- Tables
CREATE TABLE IF NOT EXISTS users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(100) NOT NULL,
    email VARCHAR(100) NOT NULL,
    password TEXT NOT NULL, -- PHC-formatted password. Upper character limit is difficult to get right.
    UNIQUE (username),
    UNIQUE (email)
);

-- User. The user is created by the container by specifying the adequate env variables.
-- We avoid using MYSQL_DATABASE to minimize the amount of grants we give to the user.
GRANT SELECT, INSERT, UPDATE ON servertech_chat.* TO 'servertech_user'@'%';
FLUSH PRIVILEGES;
