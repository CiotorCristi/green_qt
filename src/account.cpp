#include "account.h"
#include "ga.h"
#include "json.h"
#include "transaction.h"
#include "wallet.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>

#include <gdk.h>

Account::Account(Wallet* wallet)
    : QObject(wallet)
    , m_wallet(wallet)
{
}

QString Account::name() const
{
    QString name = m_json.value("name").toString();
    if (name.isEmpty()) name = "Main Account";
    return name.toUpper();
}

QJsonObject Account::json() const
{
    return m_json;
}

QQmlListProperty<Transaction> Account::transactions()
{
    return QQmlListProperty<Transaction>(this, &m_transactions,
        [](QQmlListProperty<Transaction>* property) { return static_cast<QVector<Transaction*>*>(property->data)->size(); },
    [](QQmlListProperty<Transaction>* property, int index) { return static_cast<QVector<Transaction*>*>(property->data)->at(index); });
}

void Account::update(const QJsonObject& json)
{
    m_json = json;
    m_pointer = m_json.value("pointer").toInt();
    qDebug() << "ACCOUNT UPDATE DATA:" << json;
    emit jsonChanged();
}

void Account::handleNotification(const QJsonObject &notification)
{
    reload();
}

static QJsonArray get_transactions(GA_session* session, int subaccount, int first, int count)
{
    GA_json* details = Json::fromObject({
        { "subaccount", subaccount },
        { "first", first },
        { "count", count }
    });
    GA_json* txs;
    int err = GA_get_transactions(session, details, &txs);
    Q_ASSERT(err == GA_OK);
    err = GA_destroy_json(details);
    Q_ASSERT(err == GA_OK);
    QJsonArray transactions = Json::toArray(txs);
    err = GA_destroy_json(txs);
    Q_ASSERT(err == GA_OK);
    return transactions;
}

void Account::reload()
{
    QMetaObject::invokeMethod(m_wallet->m_context, [this] {
        auto transactions = get_transactions(m_wallet->m_session, m_pointer, 0, 30);

        QMetaObject::invokeMethod(this, [this, transactions] {
            m_transactions.clear();
            for (auto value : transactions) {
                QJsonObject data = value.toObject();
                auto hash = data.value("txhash").toString();
                auto transaction = m_transactions_by_hash.value(hash);
                if (!transaction) {
                    transaction = new Transaction(this);
                    m_transactions_by_hash.insert(hash, transaction);
                }
                transaction->updateFromData(data);
                m_transactions.append(transaction);
            }
            emit transactionsChanged();
        }, Qt::BlockingQueuedConnection);
    });
}


Wallet *Account::wallet() const
{
    return m_wallet;
}

bool Account::isMainAccount() const
{
    return m_json.value("name").toString() == "";
}

ReceiveAddress::ReceiveAddress(QObject *parent) : QObject(parent)
{

}

Account *ReceiveAddress::account() const
{
    return m_account;
}

void ReceiveAddress::setAccount(Account *account)
{
    if (m_account == account) return;

    m_account = account;
    emit accountChanged(m_account);

    generate();
}

QString ReceiveAddress::address() const
{
    return m_address;
}

bool ReceiveAddress::generating() const
{
    return m_generating;
}

void ReceiveAddress::generate()
{
    if (!m_account && !m_address.isEmpty()) {
        m_address.clear();
        emit addressChanged(m_address);
        return;
    }

    if (m_generating) return;

    m_generating = true;
    emit generatingChanged(true);

    QMetaObject::invokeMethod(m_account->m_wallet->m_context, [this] {
        auto address_details = Json::fromObject({
            { "subaccount", static_cast<qint64>(m_account->m_pointer) },
        });

        GA_json* address{nullptr};
        int err = GA_get_receive_address(m_account->m_wallet->m_session, address_details, &address);
        GA_destroy_json(address_details);

        if (err != GA_OK) return;

        m_address = Json::toObject(address).value("address").toString();
        GA_destroy_json(address);

        m_generating = false;
        emit generatingChanged(true);
        emit addressChanged(m_address);
    });
}

void ReceiveAddress::copyToClipboard()
{
    QGuiApplication::clipboard()->setText(m_address);
}
