# Day15 MVC Login and Video Pagination Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将现有 Qt 登录和视频查询拆分为 Model-Service-View，加入懒汉式窗口管理器和视频文件名分页。

**Architecture:** View 只处理界面状态和信号，Service 组织业务流程，Model 执行参数化 SQL。WindowManager 作为懒汉式单例统一导航并保证 QWidget 在 QApplication 之后创建。

**Tech Stack:** C++11, Qt 5.9.8 Widgets/Sql, qmake, MinGW 5.3 32-bit, MySQL 5.7

**Spec:** `docs/superpowers/specs/2026-08-26-day15-mvc-login-video-design.md`

## Global Constraints

- 不引入新第三方依赖。
- 保留现有验证码、MD5 课程规则和主界面样式。
- 视频列表每页固定 5 条，只显示 `video_name`。
- 目标不是 Git 仓库，因此不执行提交步骤。
- 用户已批准不新建 QtTest 工程；验证使用编译、SQL 结果和后续手工 GUI 测试。

---

### Task 1: User Model and Service

**Files:**
- Modify: `model/usermodel.h`
- Modify: `model/usermodel.cpp`
- Create: `service/userservice.h`
- Create: `service/userservice.cpp`

**Interfaces:**
- Produces: `UserModel::queryUserAndPwd(QString &, QString &, int &)`
- Produces: `UserModel::updateLoginTime(int &)`
- Produces: `UserService::login(const QString &, const QString &)` returning `LoginResult`

- [ ] Implement parameter-bound `admin_user` query with `status = 1`.
- [ ] Implement `last_login_time = NOW()` update and error text propagation.
- [ ] Move MD5 hashing and login sequence into `UserService`.
- [ ] Compile these translation units through the complete project build.

### Task 2: Decouple Login View and Main View

**Files:**
- Modify: `view/loginwidget.h`
- Modify: `view/loginwidget.cpp`
- Modify: `view/mainwidget.h`
- Modify: `view/mainwidget.cpp`

**Interfaces:**
- Consumes: `UserService::login()`
- Produces: `LoginWidget::login_success(const QString &)`
- Produces: `MainWidget::login_requested()` and `MainWidget::setLoggedInUser(const QString &)`

- [ ] Remove SQL, MD5 and DbConn dependencies from `LoginWidget`.
- [ ] Preserve validation and message behavior while calling `UserService`.
- [ ] Remove `LoginWidget` ownership and direct access from `MainWidget`.
- [ ] Replace direct window switching with signals and a public login-result slot.

### Task 3: Lazy Window Manager

**Files:**
- Create: `manager/windowmanager.h`
- Create: `manager/windowmanager.cpp`
- Modify: `main.cpp`

**Interfaces:**
- Produces: `WindowManager::getInstance()`
- Produces: `WindowManager::showMainWindow()`
- Produces: `WindowManager::releaseInstance()`

- [ ] Create MainWidget and LoginWidget only inside the lazy manager constructor.
- [ ] Connect login request, success and cancellation through WindowManager.
- [ ] Release windows before releasing DbConn and before QApplication destruction.

### Task 4: Video Model, Service and Pagination UI

**Files:**
- Create: `model/videomodel.h`
- Create: `model/videomodel.cpp`
- Create: `service/videoservice.h`
- Create: `service/videoservice.cpp`
- Modify: `view/mainwidget.h`
- Modify: `view/mainwidget.cpp`

**Interfaces:**
- Produces: `VideoModel::countVideos()` and `VideoModel::queryVideoFileNames()`
- Produces: `VideoService::loadPage(int, VideoPage &, QString &)`
- Consumes: `VideoPage` in MainWidget pagination slots

- [ ] Implement count and ordered LIMIT/OFFSET queries.
- [ ] Implement fixed page size 5 and page clamping.
- [ ] Replace QSqlQueryModel/QTableView with QListWidget filename list.
- [ ] Add previous, next, refresh and page-status behavior.
- [ ] Keep queries disabled until login succeeds.

### Task 5: Seed SQL and qmake Configuration

**Files:**
- Create: `database/day15_seed.sql`
- Modify: `day15_pro.pro`
- Modify: `util/dbconn.cpp`

**Interfaces:**
- Produces: idempotent 12-row video demo dataset
- Produces: active DbConn lazy creation after QApplication startup

- [ ] Add every new header/source to qmake.
- [ ] Convert active DbConn from eager allocation to first-use allocation.
- [ ] Create 12 `day15_demo_*.mp4` rows using existing camera foreign keys.
- [ ] Execute the seed script twice and verify the second run adds zero rows.

### Task 6: Build, Database Verification and Learning Document

**Files:**
- Create: `E:/QT/LESSON15/15.QtMVC设计模式_结合案例复习总结.md`

- [ ] Run qmake with the Qt 5.9.8 kit.
- [ ] Run a clean MinGW build and require exit code 0.
- [ ] Query seeded rows with page-sized SQL and verify ordering/count.
- [ ] Check UTF-8, balanced fences, qmake source coverage and absence of stale SQL in views.
- [ ] Synchronize only intended source/docs/database files back to the E drive project.
- [ ] Write the case-driven document with file responsibilities, call flows, review guide and self-test questions.

