# Day15 MVC 登录与视频分页设计

## 目标

在不改变现有登录界面主要交互和主界面样式的前提下，完成两道编程题：

1. 使用 Model-Service-View 分层重构登录流程，并用懒汉式 `WindowManager` 统一管理登录窗口和主窗口。
2. 回放页从 MySQL `video` 表查询视频文件名，每页显示 5 条，支持上一页、下一页和刷新。

## 范围约束

- 使用课程现有 Qt 5.9.8、qmake、MinGW 32-bit 和 MySQL 5.7。
- 不实现注册、真实视频播放、搜索、排序选项或通用 ORM。
- 不引入新三方库或独立 QtTest 工程。
- 保留已有验证码、密码 MD5 课程规则、登录提示和登录后自动加载回放列表的行为。

## 架构

```text
LoginWidget
    -> UserService::login()
        -> UserModel::queryUserAndPwd()
        -> UserModel::updateLoginTime()
            -> DbConn -> MySQL

MainWidget
    -> VideoService::loadPage()
        -> VideoModel::countVideos()/queryVideoFileNames()
            -> DbConn -> MySQL

MainWidget -- login_requested --> WindowManager -- show --> LoginWidget
LoginWidget -- login_success(username) --> WindowManager --> MainWidget::setLoggedInUser()
```

## 组件设计

### UserModel

- 实现用户名与 MD5 密码的参数化查询。
- 查询成功时返回用户 ID 和数据库中的用户名。
- 登录成功后更新 `last_login_time`。
- 保存最近一次 SQL/连接错误，供 Service 区分“未找到用户”与“数据库失败”。

### UserService

- 接收界面传入的用户名和明文密码。
- 在 Service 中完成 MD5，Model 只接收数据库存储格式。
- 返回 `Success`、`InvalidCredentials` 或 `DatabaseError`。
- `last_login_time` 更新失败时返回数据库错误，不静默进入主界面。

### LoginWidget

- 仅负责输入、空值检查、验证码、消息框和发出信号。
- 不再包含 `QSqlQuery`、SQL 或 MD5 实现。
- 成功信号直接携带已确认的用户名。

### WindowManager

- 懒汉式单例，`getInstance()` 首次调用时创建。
- `main()` 必须在构造 `QApplication` 之后才调用 `getInstance()`。
- 管理 `MainWidget` 与 `LoginWidget` 的创建、信号连接、显示、隐藏和销毁。
- `main()` 在 `app.exec()` 返回后先释放 WindowManager，再释放数据库连接。

### VideoModel / VideoService

- `VideoModel` 查询总记录数，并按 `start_time DESC, id DESC` 分页查询 `video_name`。
- `VideoService` 固定每页 5 条，计算总页数、校正越界页码并返回 `VideoPage` DTO。
- `MainWidget` 仅将文件名放入 `QListWidget`，更新页码文字和按钮可用状态。

## 初始数据

`database/day15_seed.sql` 为 4 个已有摄像头插入 12 条 `day15_demo_01.mp4` 至 `day15_demo_12.mp4` 记录。每条使用 `WHERE NOT EXISTS` 判断 `video_path`，重复执行不会重复写入。

## 错误处理

- 账号不存在与密码错误统一提示“用户名或密码错误”。
- 数据库连接、SQL 执行和更新登录时间失败时，显示具体数据库错误。
- 未登录时禁止查询视频列表。
- 无视频数据时显示空列表与 `第 1 / 1 页，共 0 条`。

## 验证

- 对 SQL 初始化脚本执行两次，演示数据条数不在第二次增加。
- qmake 重新生成 Makefile，使用 Qt 5.9.8 MinGW 32-bit 完整构建。
- 使用 MySQL CLI 验证演示视频数量、按页查询结果和 `admin_user` 登录时间字段。
- 图形界面的账号、验证码和分页点击测试由用户后续手动执行。

