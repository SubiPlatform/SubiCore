#include "subinode.h"
#include "qt/forms/ui_subinode.h"

#include "subinode/activesubinode.h"
#include "clientmodel.h"
#include "init.h"
#include "guiutil.h"
#include "subinode/subinode-sync.h"
#include "subinode/subinodeconfig.h"
#include "subinode/subinodeman.h"
#include "sync.h"
#include "wallet/wallet.h"
#include "walletmodel.h"
#include <boost/foreach.hpp>

#include <QTimer>
#include <QMessageBox>

int GetOffsetFromUtc()
{
#if QT_VERSION < 0x050200
    const QDateTime dateTime1 = QDateTime::currentDateTime();
    const QDateTime dateTime2 = QDateTime(dateTime1.date(), dateTime1.time(), Qt::UTC);
    return dateTime1.secsTo(dateTime2);
#else
    return QDateTime::currentDateTime().offsetFromUtc();
#endif
}

SubiNode::SubiNode(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SubiNode),
    clientModel(0),
    walletModel(0)
{
    ui->setupUi(this);

    ui->startButton->setEnabled(false);

    int columnAliasWidth = 100;
    int columnAddressWidth = 200;
    int columnProtocolWidth = 60;
    int columnStatusWidth = 80;
    int columnActiveWidth = 130;
    int columnLastSeenWidth = 130;

    ui->tableWidgetMySubinodes->setColumnWidth(0, columnAliasWidth);
    ui->tableWidgetMySubinodes->setColumnWidth(1, columnAddressWidth);
    ui->tableWidgetMySubinodes->setColumnWidth(2, columnProtocolWidth);
    ui->tableWidgetMySubinodes->setColumnWidth(3, columnStatusWidth);
    ui->tableWidgetMySubinodes->setColumnWidth(4, columnActiveWidth);
    ui->tableWidgetMySubinodes->setColumnWidth(5, columnLastSeenWidth);

    ui->tableWidgetSubinodes->setColumnWidth(0, columnAddressWidth);
    ui->tableWidgetSubinodes->setColumnWidth(1, columnProtocolWidth);
    ui->tableWidgetSubinodes->setColumnWidth(2, columnStatusWidth);
    ui->tableWidgetSubinodes->setColumnWidth(3, columnActiveWidth);
    ui->tableWidgetSubinodes->setColumnWidth(4, columnLastSeenWidth);

    ui->tableWidgetMySubinodes->setContextMenuPolicy(Qt::CustomContextMenu);

    QAction *startAliasAction = new QAction(tr("Start alias"), this);
    contextMenu = new QMenu();
    contextMenu->addAction(startAliasAction);
    connect(ui->tableWidgetMySubinodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(startAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    fFilterUpdated = false;
    nTimeFilterUpdated = GetTime();
    updateNodeList();
}

SubiNode::~SubiNode()
{
    delete ui;
}

void SubiNode::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model) {
        // try to update list when Subinode count changes
        connect(clientModel, SIGNAL(strSubinodesChanged(QString)), this, SLOT(updateNodeList()));
    }
}

void SubiNode::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void SubiNode::showContextMenu(const QPoint &point)
{
    QTableWidgetItem *item = ui->tableWidgetMySubinodes->itemAt(point);
    if(item) contextMenu->exec(QCursor::pos());
}

void SubiNode::StartAlias(std::string strAlias)
{
    std::string strStatusHtml;
    strStatusHtml += "<center>Alias: " + strAlias;

    BOOST_FOREACH(CSubinodeConfig::CSubinodeEntry mne, subinodeConfig.getEntries()) {
        if(mne.getAlias() == strAlias) {
            std::string strError;
            CSubinodeBroadcast mnb;

            bool fSuccess = CSubinodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

            if(fSuccess) {
                strStatusHtml += "<br>Successfully started subinode.";
                mnodeman.UpdateSubinodeList(mnb);
                mnb.RelaySubiNode();
                mnodeman.NotifySubinodeUpdates();
            } else {
                strStatusHtml += "<br>Failed to start subinode.<br>Error: " + strError;
            }
            break;
        }
    }
    strStatusHtml += "</center>";

    QMessageBox msg;
    msg.setText(QString::fromStdString(strStatusHtml));
    msg.exec();

    updateMyNodeList(true);
}

void SubiNode::StartAll(std::string strCommand)
{
    int nCountSuccessful = 0;
    int nCountFailed = 0;
    std::string strFailedHtml;

    BOOST_FOREACH(CSubinodeConfig::CSubinodeEntry mne, subinodeConfig.getEntries()) {
        std::string strError;
        CSubinodeBroadcast mnb;

        int32_t nOutputIndex = 0;
        if(!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
            continue;
        }

        COutPoint outpoint = COutPoint(uint256S(mne.getTxHash()), nOutputIndex);

        if(strCommand == "start-missing" && mnodeman.Has(CTxIn(outpoint))) continue;

        bool fSuccess = CSubinodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

        if(fSuccess) {
            nCountSuccessful++;
            mnodeman.UpdateSubinodeList(mnb);
            mnb.RelaySubiNode();
            mnodeman.NotifySubinodeUpdates();
        } else {
            nCountFailed++;
            strFailedHtml += "\nFailed to start " + mne.getAlias() + ". Error: " + strError;
        }
    }
    walletModel->getWallet()->Lock();

    std::string returnObj;
    returnObj = strprintf("Successfully started %d subinodes, failed to start %d, total %d", nCountSuccessful, nCountFailed, nCountFailed + nCountSuccessful);
    if (nCountFailed > 0) {
        returnObj += strFailedHtml;
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(returnObj));
    msg.exec();

    updateMyNodeList(true);
}

void SubiNode::updateMySubinodeInfo(QString strAlias, QString strAddr, const COutPoint& outpoint)
{
    bool fOldRowFound = false;
    int nNewRow = 0;

    for(int i = 0; i < ui->tableWidgetMySubinodes->rowCount(); i++) {
        if(ui->tableWidgetMySubinodes->item(i, 0)->text() == strAlias) {
            fOldRowFound = true;
            nNewRow = i;
            break;
        }
    }

    if(nNewRow == 0 && !fOldRowFound) {
        nNewRow = ui->tableWidgetMySubinodes->rowCount();
        ui->tableWidgetMySubinodes->insertRow(nNewRow);
    }

    subinode_info_t infoMn = mnodeman.GetSubinodeInfo(CTxIn(outpoint));
    bool fFound = infoMn.fInfoValid;

    QTableWidgetItem *aliasItem = new QTableWidgetItem(strAlias);
    QTableWidgetItem *addrItem = new QTableWidgetItem(fFound ? QString::fromStdString(infoMn.addr.ToString()) : strAddr);
    QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(fFound ? infoMn.nProtocolVersion : -1));
    QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(fFound ? CSubinode::StateToString(infoMn.nActiveState) : "MISSING"));
    QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(fFound ? (infoMn.nTimeLastPing - infoMn.sigTime) : 0)));
    QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M",
                                                                                                   fFound ? infoMn.nTimeLastPing + GetOffsetFromUtc() : 0)));
    QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(fFound ? CBitcoinAddress(infoMn.pubKeyCollateralAddress.GetID()).ToString() : ""));

    ui->tableWidgetMySubinodes->setItem(nNewRow, 0, aliasItem);
    ui->tableWidgetMySubinodes->setItem(nNewRow, 1, addrItem);
    ui->tableWidgetMySubinodes->setItem(nNewRow, 2, protocolItem);
    ui->tableWidgetMySubinodes->setItem(nNewRow, 3, statusItem);
    ui->tableWidgetMySubinodes->setItem(nNewRow, 4, activeSecondsItem);
    ui->tableWidgetMySubinodes->setItem(nNewRow, 5, lastSeenItem);
    ui->tableWidgetMySubinodes->setItem(nNewRow, 6, pubkeyItem);
}

void SubiNode::updateMyNodeList(bool fForce)
{
    TRY_LOCK(cs_mymnlist, fLockAcquired);
    if(!fLockAcquired) {
        return;
    }
    static int64_t nTimeMyListUpdated = 0;

    // automatically update my subinode list only once in MY_MASTERNODELIST_UPDATE_SECONDS seconds,
    // this update still can be triggered manually at any time via button click
    int64_t nSecondsTillUpdate = nTimeMyListUpdated + MY_MASTERNODELIST_UPDATE_SECONDS - GetTime();
    ui->secondsLabel->setText(QString::number(nSecondsTillUpdate));

    if(nSecondsTillUpdate > 0 && !fForce) return;
    nTimeMyListUpdated = GetTime();

    ui->tableWidgetSubinodes->setSortingEnabled(false);
    BOOST_FOREACH(CSubinodeConfig::CSubinodeEntry mne, subinodeConfig.getEntries()) {
        int32_t nOutputIndex = 0;
        if(!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
            continue;
        }

        updateMySubinodeInfo(QString::fromStdString(mne.getAlias()), QString::fromStdString(mne.getIp()), COutPoint(uint256S(mne.getTxHash()), nOutputIndex));
    }
    ui->tableWidgetSubinodes->setSortingEnabled(true);

    // reset "timer"
    ui->secondsLabel->setText("0");
}

void SubiNode::updateNodeList()
{
    TRY_LOCK(cs_mnlist, fLockAcquired);
    if(!fLockAcquired) {
        return;
    }

    static int64_t nTimeListUpdated = GetTime();

    // to prevent high cpu usage update only once in MASTERNODELIST_UPDATE_SECONDS seconds
    // or MASTERNODELIST_FILTER_COOLDOWN_SECONDS seconds after filter was last changed
    int64_t nSecondsToWait = fFilterUpdated
                            ? nTimeFilterUpdated - GetTime() + MASTERNODELIST_FILTER_COOLDOWN_SECONDS
                            : nTimeListUpdated - GetTime() + MASTERNODELIST_UPDATE_SECONDS;

    if(fFilterUpdated) ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", nSecondsToWait)));
    if(nSecondsToWait > 0) return;

    nTimeListUpdated = GetTime();
    fFilterUpdated = false;

    QString strToFilter;
    ui->countLabel->setText("Updating...");
    ui->tableWidgetSubinodes->setSortingEnabled(false);
    ui->tableWidgetSubinodes->clearContents();
    ui->tableWidgetSubinodes->setRowCount(0);
//    std::map<COutPoint, CSubinode> mapSubinodes = mnodeman.GetFullSubinodeMap();
    std::vector<CSubinode> vSubinodes = mnodeman.GetFullSubinodeVector();
    int offsetFromUtc = GetOffsetFromUtc();

    BOOST_FOREACH(CSubinode & mn, vSubinodes)
    {
//        CSubinode mn = mnpair.second;
        // populate list
        // Address, Protocol, Status, Active Seconds, Last Seen, Pub Key
        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(mn.addr.ToString()));
        QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(mn.nProtocolVersion));
        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(mn.GetStatus()));
        QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(mn.lastPing.sigTime - mn.sigTime)));
        QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M", mn.lastPing.sigTime + offsetFromUtc)));
        QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString()));

        if (strCurrentFilter != "")
        {
            strToFilter =   addressItem->text() + " " +
                            protocolItem->text() + " " +
                            statusItem->text() + " " +
                            activeSecondsItem->text() + " " +
                            lastSeenItem->text() + " " +
                            pubkeyItem->text();
            if (!strToFilter.contains(strCurrentFilter)) continue;
        }

        ui->tableWidgetSubinodes->insertRow(0);
        ui->tableWidgetSubinodes->setItem(0, 0, addressItem);
        ui->tableWidgetSubinodes->setItem(0, 1, protocolItem);
        ui->tableWidgetSubinodes->setItem(0, 2, statusItem);
        ui->tableWidgetSubinodes->setItem(0, 3, activeSecondsItem);
        ui->tableWidgetSubinodes->setItem(0, 4, lastSeenItem);
        ui->tableWidgetSubinodes->setItem(0, 5, pubkeyItem);
    }

    ui->countLabel->setText(QString::number(ui->tableWidgetSubinodes->rowCount()));
    ui->tableWidgetSubinodes->setSortingEnabled(true);
}

void SubiNode::on_filterLineEdit_textChanged(const QString &strFilterIn)
{
    strCurrentFilter = strFilterIn;
    nTimeFilterUpdated = GetTime();
    fFilterUpdated = true;
    ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", MASTERNODELIST_FILTER_COOLDOWN_SECONDS)));
}

void SubiNode::on_startButton_clicked()
{
    std::string strAlias;
    {
        LOCK(cs_mymnlist);
        // Find selected node alias
        QItemSelectionModel* selectionModel = ui->tableWidgetMySubinodes->selectionModel();
        QModelIndexList selected = selectionModel->selectedRows();

        if(selected.count() == 0) return;

        QModelIndex index = selected.at(0);
        int nSelectedRow = index.row();
        strAlias = ui->tableWidgetMySubinodes->item(nSelectedRow, 0)->text().toStdString();
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm subinode start"),
        tr("Are you sure you want to start subinode %1?").arg(QString::fromStdString(strAlias)),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAlias(strAlias);
        return;
    }

    StartAlias(strAlias);
}

void SubiNode::on_startAllButton_clicked()
{
    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm all subinode start"),
        tr("Are you sure you want to start ALL subinodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll();
        return;
    }

    StartAll();
}

void SubiNode::on_startMissingButton_clicked()
{

    if(!subinodeSync.IsSubinodeListSynced()) {
        QMessageBox::critical(this, tr("Command is not available right now"),
            tr("You can't use this command until subinode list is synced"));
        return;
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this,
        tr("Confirm missing subinodes start"),
        tr("Are you sure you want to start MISSING subinodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll("start-missing");
        return;
    }

    StartAll("start-missing");
}

void SubiNode::on_tableWidgetMySubinodes_itemSelectionChanged()
{
    if(ui->tableWidgetMySubinodes->selectedItems().count() > 0) {
        ui->startButton->setEnabled(true);
    }
}

void SubiNode::on_UpdateButton_clicked()
{
    updateMyNodeList(true);
}
