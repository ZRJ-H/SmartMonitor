// 认证策略单元测试：覆盖账号/密码边界、三次失败锁定、成功后清零和访客权限。
#include <QtTest>

#include "../service/authpolicy.h"
#include "../service/authsession.h"

class Day2AuthTests : public QObject
{
    Q_OBJECT

private slots:
    // 验证用户名与密码长度、字符范围的边界条件。
    void validatesAccountAndPasswordBoundaries();
    // 验证第三次失败触发十分钟锁定，并在到期后自动解除。
    void locksAfterThirdFailureAndUnlocksAfterTenMinutes();
    // 验证成功登录会清除该账号之前累计的失败次数。
    void successfulLoginClearsFailures();
    // 验证访客只能访问实时预览，回放和设置需要登录。
    void guestCanOnlyAccessLivePreview();
};

void Day2AuthTests::validatesAccountAndPasswordBoundaries()
{
    QVERIFY(AuthPolicy::isUserNameValid("admin1"));
    QVERIFY(AuthPolicy::isUserNameValid("admin_0001"));
    QVERIFY(!AuthPolicy::isUserNameValid("admin"));
    QVERIFY(!AuthPolicy::isUserNameValid("admin_00001"));
    QVERIFY(!AuthPolicy::isUserNameValid("admin-01"));

    QVERIFY(AuthPolicy::isPasswordValid("123456"));
    QVERIFY(AuthPolicy::isPasswordValid("12345678"));
    QVERIFY(!AuthPolicy::isPasswordValid("12345"));
    QVERIFY(!AuthPolicy::isPasswordValid("123456789"));
    QVERIFY(!AuthPolicy::isPasswordValid("12 456"));
}

void Day2AuthTests::locksAfterThirdFailureAndUnlocksAfterTenMinutes()
{
    LoginThrottle throttle;
    const QDateTime start(QDate(2026, 9, 18), QTime(9, 0, 0));

    LoginFailure first = throttle.recordFailure("admin1", start);
    QCOMPARE(first.remainingAttempts, 2);
    QVERIFY(!first.locked);

    LoginFailure second = throttle.recordFailure("admin1", start.addSecs(1));
    QCOMPARE(second.remainingAttempts, 1);
    QVERIFY(!second.locked);

    LoginFailure third = throttle.recordFailure("admin1", start.addSecs(2));
    QCOMPARE(third.remainingAttempts, 0);
    QVERIFY(third.locked);
    QCOMPARE(third.lockSeconds, 600);

    int remainingSeconds = 0;
    QVERIFY(throttle.isLocked("admin1", start.addSecs(601), &remainingSeconds));
    QCOMPARE(remainingSeconds, 1);
    QVERIFY(!throttle.isLocked("admin1", start.addSecs(602), &remainingSeconds));

    LoginFailure afterExpiry = throttle.recordFailure("admin1", start.addSecs(603));
    QCOMPARE(afterExpiry.remainingAttempts, 2);
    QVERIFY(!afterExpiry.locked);
}

void Day2AuthTests::successfulLoginClearsFailures()
{
    LoginThrottle throttle;
    const QDateTime now(QDate(2026, 9, 18), QTime(10, 0, 0));

    throttle.recordFailure("admin1", now);
    throttle.recordFailure("admin1", now.addSecs(1));
    throttle.recordSuccess("admin1");

    const LoginFailure next = throttle.recordFailure("admin1", now.addSecs(2));
    QCOMPARE(next.remainingAttempts, 2);
    QVERIFY(!next.locked);
}

void Day2AuthTests::guestCanOnlyAccessLivePreview()
{
    AuthSession session;
    QVERIFY(session.canAccess(AuthPermission::LivePreview));
    QVERIFY(!session.canAccess(AuthPermission::Playback));
    QVERIFY(!session.canAccess(AuthPermission::Settings));

    session.login("admin1");
    QVERIFY(session.canAccess(AuthPermission::Playback));
    QVERIFY(session.canAccess(AuthPermission::Settings));
    QCOMPARE(session.userName(), QString("admin1"));

    session.logout();
    QVERIFY(!session.isLoggedIn());
    QVERIFY(session.userName().isEmpty());
    QVERIFY(!session.canAccess(AuthPermission::Playback));
}

QTEST_APPLESS_MAIN(Day2AuthTests)
#include "tst_day2_auth.moc"
