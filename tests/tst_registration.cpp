#include <QtTest>
#include <QSqlQuery>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include "../view/loginwidget.h"
#include "../service/userservice.h"
#include "../util/dbconn.h"
class RegistrationTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        qputenv("SMART_MONITOR_DB_DRIVER", "QSQLITE");
        qputenv("SMART_MONITOR_DB_NAME", ":memory:");
        QSqlQuery q(DbConn::getInstance()->database());
        QVERIFY(q.exec("CREATE TABLE admin_user (id INTEGER PRIMARY KEY, username TEXT UNIQUE COLLATE NOCASE, password TEXT NOT NULL, status INTEGER DEFAULT 1, last_login_time TEXT)"));
    }
    void validation() {
        UserService s;
        QCOMPARE(s.registerUser("bad", "abcdef", "abcdef").status, RegistrationResult::InvalidUserName);
        QCOMPARE(s.registerUser("tester01", "a b c d", "a b c d").status, RegistrationResult::InvalidPassword);
        QCOMPARE(s.registerUser("tester01", "abcdef", "abcdeg").status, RegistrationResult::PasswordMismatch);
    }
    void registerAndLogin() {
        UserService s;
        QCOMPARE(s.registerUser(" tester01 ", "abcdef", "abcdef").status, RegistrationResult::Success);
        QCOMPARE(s.registerUser("TESTER01", "ghijkl", "ghijkl").status, RegistrationResult::DuplicateUser);
        QCOMPARE(s.login("tester01", "abcdef").status, LoginResult::Success);
        QSqlQuery q(DbConn::getInstance()->database());
        QVERIFY(q.exec("SELECT password, last_login_time FROM admin_user WHERE username='tester01'"));
        QVERIFY(q.next()); QVERIFY(q.value(0).toString() != "abcdef"); QVERIFY(!q.value(1).isNull());
        QVERIFY(q.exec("UPDATE admin_user SET status=0 WHERE username='tester01'"));
        QCOMPARE(s.registerUser("tester01", "ghijkl", "ghijkl").status, RegistrationResult::DuplicateUser);
    }
    void buttonFlow() {
        LoginWidget w;
        QSignalSpy loggedIn(&w, &LoginWidget::login_success);
        QSignalSpy cancelled(&w, &LoginWidget::login_cancelled);
        bool submitted = false;
        QTimer::singleShot(0, &w, [&] {
            auto *d = w.findChild<QDialog *>("registrationDialog");
            if (!d) { if (auto *modal = QApplication::activeModalWidget()) modal->close(); return; }
            d->findChild<QLineEdit *>("registrationName")->setText("newuser1");
            d->findChild<QLineEdit *>("registrationPassword")->setText("abcdef");
            d->findChild<QLineEdit *>("registrationConfirmation")->setText("abcdef");
            d->findChild<QPushButton *>("registrationSubmit")->click();
            submitted = d->result() == QDialog::Accepted;
            if (!submitted) d->reject();
        });
        w.findChild<QPushButton *>("registerButton")->click();
        QVERIFY(submitted);
        QCOMPARE(w.findChild<QLineEdit *>("loginUserName")->text(), QString("newuser1"));
        QVERIFY(w.findChild<QLineEdit *>("loginPassword")->text().isEmpty());
        QCOMPARE(loggedIn.count(), 0); QCOMPARE(cancelled.count(), 0);
        UserService s; QCOMPARE(s.login("newuser1", "abcdef").status, LoginResult::Success);
    }
    void databaseFailure() {
        QSqlQuery q(DbConn::getInstance()->database()); QVERIFY(q.exec("DROP TABLE admin_user"));
        UserService s; QCOMPARE(s.registerUser("tester02", "abcdef", "abcdef").status, RegistrationResult::DatabaseError);
    }
    void cleanupTestCase() { DbConn::releaseDbConn(); }
};
QTEST_MAIN(RegistrationTests)
#include "tst_registration.moc"
