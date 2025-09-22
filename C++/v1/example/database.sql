1. 用户表（sys_user）
CREATE TABLE `sys_user` (
  `user_id` bigint NOT NULL AUTO_INCREMENT COMMENT '用户ID',
  `username` varchar(50) NOT NULL COMMENT '登录账号',
  `password` varchar(255) NOT NULL COMMENT '加密密码（bcrypt）',
  `email` varchar(100) DEFAULT NULL COMMENT '邮箱',
  `phone` varchar(20) DEFAULT NULL COMMENT '手机号',
  `nickname` varchar(50) DEFAULT NULL COMMENT '用户昵称',
  `status` tinyint NOT NULL DEFAULT 1 COMMENT '状态（1-正常，0-禁用）',
  `last_login_ip` varchar(50) DEFAULT NULL COMMENT '最后登录IP',
  `last_login_time` datetime DEFAULT NULL COMMENT '最后登录时间',
  `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
  `update_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`user_id`),
  UNIQUE KEY `uk_username` (`username`),  -- 确保账号唯一
  UNIQUE KEY `uk_email` (`email`),        -- 确保邮箱唯一（若启用）
  UNIQUE KEY `uk_phone` (`phone`)         -- 确保手机号唯一（若启用）
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='系统用户表';

角色表（sys_role）
CREATE TABLE `sys_role` (
  `role_id` bigint NOT NULL AUTO_INCREMENT COMMENT '角色ID',
  `role_name` varchar(50) NOT NULL COMMENT '角色名称',
  `role_desc` varchar(200) DEFAULT NULL COMMENT '角色描述',
  `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
  PRIMARY KEY (`role_id`),
  UNIQUE KEY `uk_role_name` (`role_name`)  -- 角色名称唯一
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='角色表';

用户角色关联表（sys_user_role）
CREATE TABLE `sys_user_role` (
  `id` bigint NOT NULL AUTO_INCREMENT COMMENT '关联ID',
  `user_id` bigint NOT NULL COMMENT '用户ID',
  `role_id` bigint NOT NULL COMMENT '角色ID',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_user_role` (`user_id`,`role_id`),  -- 避免重复关联
  KEY `fk_role_id` (`role_id`),
  CONSTRAINT `fk_user_id` FOREIGN KEY (`user_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE,
  CONSTRAINT `fk_role_id` FOREIGN KEY (`role_id`) REFERENCES `sys_role` (`role_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户角色关联表';

登录日志表（sys_login_log）

CREATE TABLE `sys_login_log` (
  `log_id` bigint NOT NULL AUTO_INCREMENT COMMENT '日志ID',
  `username` varchar(50) NOT NULL COMMENT '登录账号',
  `ip_address` varchar(50) NOT NULL COMMENT '登录IP',
  `user_agent` text COMMENT '浏览器/设备信息',
  `login_status` tinyint NOT NULL COMMENT '登录状态（1-成功，0-失败）',
  `error_msg` varchar(200) DEFAULT NULL COMMENT '错误信息',
  `login_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '登录时间',
  PRIMARY KEY (`log_id`),
  KEY `idx_username` (`username`),  -- 按账号查询日志
  KEY `idx_login_time` (`login_time`)  -- 按时间查询日志
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='登录日志表';

-- 1. 插入初始角色
INSERT INTO `sys_role` (`role_name`, `role_desc`) VALUES 
('admin', '系统管理员（拥有全部权限）'),
('user', '普通用户（基础权限）');

-- 2. 插入管理员账号（密码建议为强密码，此处仅为示例）
-- 注意：实际使用时需替换为bcrypt加密后的密码（如明文"Admin@123"加密后的值）
INSERT INTO `sys_user` (`username`, `password`, `email`, `nickname`, `status`) 
VALUES ('admin', '1234', 'admin@example.com', '系统管理员', 1);

-- 3. 关联管理员与admin角色
INSERT INTO `sys_user_role` (`user_id`, `role_id`) 
VALUES (1, 1);  -- 假设管理员user_id=1，admin角色role_id=1