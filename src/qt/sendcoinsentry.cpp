// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2014-2017 The D�sh Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sendcoinsentry.h"
#include "ui_sendcoinsentry.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "walletmodel.h"
#include "main.cpp"

#include <QApplication>
#include <QClipboard>
#include <QUrl>

QString ToQstring(std::string s);
std::string FromQStringW(QString qs);
std::string GetSin(int iSinNumber, std::string& out_Description);
bool Contains(std::string data, std::string instring);

// POG
TitheDifficultyParams GetTitheParams(const CBlockIndex* pindex);
double GetPOGDifficulty(const CBlockIndex* pindex);
// End of POG



SendCoinsEntry::SendCoinsEntry(const PlatformStyle *platformStyle, QWidget *parent) :
    QStackedWidget(parent),
    ui(new Ui::SendCoinsEntry),
    model(0),
    platformStyle(platformStyle)
{
    ui->setupUi(this);

    setCurrentWidget(ui->SendCoins);

    if (platformStyle->getUseExtraSpacing())
        ui->payToLayout->setSpacing(4);
#if QT_VERSION >= 0x040700
    ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book"));
#endif

    QString theme = GUIUtil::getThemeName();

    // These icons are needed on Mac also!
    ui->addressBookButton->setIcon(QIcon(":/icons/" + theme + "/address-book"));
    ui->pasteButton->setIcon(QIcon(":/icons/" + theme + "/editpaste"));
    ui->deleteButton->setIcon(QIcon(":/icons/" + theme + "/remove"));
    ui->deleteButton_is->setIcon(QIcon(":/icons/" + theme + "/remove"));
    ui->deleteButton_s->setIcon(QIcon(":/icons/" + theme + "/remove"));
      
    // normal biblepay address field
    GUIUtil::setupAddressWidget(ui->payTo, this);
    // just a label for displaying biblepay address(es)
    ui->payTo_is->setFont(GUIUtil::fixedPitchFont());

    // Connect signals
    connect(ui->payAmount, SIGNAL(valueChanged()), this, SIGNAL(payAmountChanged()));
    connect(ui->checkboxSubtractFeeFromAmount, SIGNAL(toggled(bool)), this, SIGNAL(subtractFeeFromAmountChanged()));
	
	connect(ui->checkboxTithe, SIGNAL(toggled(bool)), this, SIGNAL(titheChanged()));
	connect(ui->checkboxRepent, SIGNAL(toggled(bool)), this, SIGNAL(repentChanged()));
	connect(ui->checkboxFoundation, SIGNAL(toggled(bool)), this, SIGNAL(foundationChanged()));

	connect(ui->checkboxFoundation, SIGNAL(toggled(bool)), this, SLOT(updateFoundationAddress()));
	connect(ui->checkboxRepent, SIGNAL(toggled(bool)), this, SLOT(makeRepentanceVisible()));
	connect(ui->checkboxPrayer, SIGNAL(toggled(bool)), this, SLOT(checkboxPrayerClicked()));

    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->deleteButton_is, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->deleteButton_s, SIGNAL(clicked()), this, SLOT(deleteClicked()));
	// IPFS
	connect(ui->btnAttach, SIGNAL(clicked()), this, SLOT(attachFile()));
	const CChainParams& chainparams = Params();
	bool fAttachDisabled = false;
	if (fAttachDisabled)
	{
		ui->btnAttach->setVisible(false);
		ui->lblIPFSFee->setVisible(false);
		ui->txtFile->setVisible(false);
		ui->lblatt->setVisible(false);
	}

	// Initialize Repentance Combo
	initRepentanceDropDown();
	initPOGDifficulty();

}

SendCoinsEntry::~SendCoinsEntry()
{
    delete ui;
}

void SendCoinsEntry::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}


void SendCoinsEntry::updateFoundationAddress()
{
	const CChainParams& chainparams = Params();
    ui->payTo->setText(ToQstring(chainparams.GetConsensus().FoundationAddress));
	initPOGDifficulty();
    ui->payAmount->setFocus();
}

void SendCoinsEntry::checkboxPrayerClicked()
{
	// When prayer is checked, repentance is invisible:
	bool bChecked = (ui->checkboxPrayer->checkState() == Qt::Checked);
	if (bChecked)
	{
		ui->checkboxRepent->setCheckState(Qt::Unchecked);
		ui->comboRepent->setVisible(false);
		ui->lblRepent->setVisible(false);
	}
}

void SendCoinsEntry::makeRepentanceVisible()
{
	bool bChecked = (ui->checkboxRepent->checkState() == Qt::Checked);
	ui->comboRepent->setVisible(bChecked);
	ui->lblRepent->setVisible(bChecked);
	// If Repentance is visible, Prayer is unchecked
	if (bChecked)
	{
		ui->checkboxPrayer->setCheckState(Qt::Unchecked);
	}
}

void SendCoinsEntry::initPOGDifficulty()
{	// Initialize Pog difficulty (12-8-2018)
	bool bDonateFoundation = (ui->checkboxFoundation->checkState() == Qt::Checked);
	if (fPOGEnabled && bDonateFoundation)
	{
		TitheDifficultyParams tdp = GetTitheParams(chainActive.Tip());
		double pog_diff = GetPOGDifficulty(chainActive.Tip());
		std::string sValue = "POG Difficulty: " + RoundToString(pog_diff, 4) + ", MinCoinAge: " + RoundToString(tdp.min_coin_age, 4) 
			+ ", MinCoinValue: " + RoundToString((double)(tdp.min_coin_amount/COIN), 4) 
				+ ", MaxTitheAmount: " + RoundToString((double)(tdp.max_tithe_amount/COIN), 4);
		std::string sCSS = "QLabel { background-color : transparent; color: red; }";
		ui->lblPogDifficulty->setStyleSheet(ToQstring(sCSS));
		ui->lblPogDifficultyCaption->setStyleSheet(ToQstring(sCSS));
		ui->lblPogDifficultyCaption->setVisible(true);
		ui->lblPogDifficultyCaption->setText(ToQstring("POG:"));
		ui->lblPogDifficulty->setText(ToQstring(sValue));
		ui->lblPogDifficulty->setVisible(true);
		ui->lblCheckboxes->setVisible(false);

		std::map<double, CAmount> dtb = pwalletMain->GetDimensionalCoins(tdp.min_coin_age, tdp.min_coin_amount);
		CAmount nTotal = 0;
		double nQty = 0;
		double nAvgAge = 0;
		double nTotalAge = 0;
		CAmount nMaxCoin = 0;
		BOOST_FOREACH(const PAIRTYPE(double, CAmount)& item, dtb)
    	{
			CAmount nAmount = item.second;
			double dAge = item.first;
			nQty++;
			if (nAmount > nMaxCoin) nMaxCoin = nAmount;
			nTotal += nAmount;
			nTotalAge += dAge;
		}
		if (nQty > 0) nAvgAge = nTotalAge / nQty;

		CAmount nTithability = nMaxCoin;
		if (nTithability > tdp.max_tithe_amount) nTithability = tdp.max_tithe_amount;
		std::string sSummary = (nTithability > 0) ? "YES" : "NO";

		std::string sTitheValue = "Qty: " + RoundToString(nQty, 0) 
			+ ", LargeCoin: " + RoundToString((double)nMaxCoin/COIN, 2) 
			+ ", AvgAge: " 
			+ RoundToString(nAvgAge, 4) 
			+ ", TotalTitheBalance: " 
			+ RoundToString((double)nTotal/COIN, 2) + ", Tithability: " + RoundToString((double)nTithability/COIN, 4) + ", Summary: " + sSummary;
		
		ui->lblTitheAbility->setStyleSheet(ToQstring(sCSS));
		ui->lblTitheAbilityCaption->setStyleSheet(ToQstring(sCSS));
		ui->lblTitheAbilityCaption->setVisible(true);
		ui->lblTitheAbilityCaption->setText(ToQstring("Tithe Ability:"));
		ui->lblTitheAbility->setText(ToQstring(sTitheValue));
		ui->lblTitheAbility->setVisible(true);
    }
	else
	{
		ui->lblPogDifficultyCaption->setVisible(false);
		ui->lblPogDifficulty->setVisible(false);
		ui->lblTitheAbilityCaption->setVisible(false);
		ui->lblTitheAbility->setVisible(false);
		ui->lblCheckboxes->setVisible(false);
	}
}

void SendCoinsEntry::initRepentanceDropDown()
{
	for (int i = 0; i <= 38; i++)
	{
		std::string sDescription = "";
		std::string sSin = GetSin(i, sDescription);
		ui->comboRepent->addItem(ToQstring(sSin));
	}
	ui->comboRepent->setVisible(false);
	ui->lblRepent->setVisible(false);
}



void SendCoinsEntry::on_addressBookButton_clicked()
{
    if(!model)
        return;
    AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        ui->payTo->setText(dlg.getReturnValue());
        ui->payAmount->setFocus();
    }
}

void SendCoinsEntry::on_payTo_textChanged(const QString &address)
{
    updateLabel(address);
}

void SendCoinsEntry::setModel(WalletModel *model)
{
    this->model = model;

    if (model && model->getOptionsModel())
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

    clear();
}

void SendCoinsEntry::clear()
{
    // clear UI elements for normal payment
    ui->payTo->clear();
    ui->addAsLabel->clear();
    ui->payAmount->clear();
    ui->checkboxSubtractFeeFromAmount->setCheckState(Qt::Unchecked);
	ui->checkboxTithe->setCheckState(Qt::Unchecked);
    ui->messageTextLabel->clear();
    ui->messageTextLabel->hide();
    ui->messageLabel->hide();
    // clear UI elements for unauthenticated payment request
    ui->payTo_is->clear();
    ui->memoTextLabel_is->clear();
    ui->payAmount_is->clear();
    // clear UI elements for authenticated payment request
    ui->payTo_s->clear();
    ui->memoTextLabel_s->clear();
    ui->payAmount_s->clear();
	ui->txtFile->clear();
    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void SendCoinsEntry::attachFile()
{
    QString filename = GUIUtil::getOpenFileName(this, tr("Select a file to attach to this transaction"), "", "", NULL);
    if(filename.isEmpty()) return;
    QUrl fileUri = QUrl::fromLocalFile(filename);
	std::string sFN = FromQStringW(fileUri.toString());
	// 8-30-2018 
	bool bFromWindows = Contains(sFN, "file:///C:") || Contains(sFN, "file:///D:") || Contains(sFN, "file:///E:");
	if (!bFromWindows)
	{
		sFN = strReplace(sFN, "file://", "");  // This leaves the full unix path
	}
	else
	{
		sFN = strReplace(sFN, "file:///", "");  // This leaves the windows drive letter
	}
	
    ui->txtFile->setText(ToQstring(sFN));
}

void SendCoinsEntry::deleteClicked()
{
    Q_EMIT removeEntry(this);
}

bool SendCoinsEntry::validate()
{
    if (!model)
        return false;

    // Check input validity
    bool retval = true;

    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;

    if (!model->validateAddress(ui->payTo->text()))
    {
        ui->payTo->setValid(false);
        retval = false;
    }

    if (!ui->payAmount->validate())
    {
        retval = false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0)
    {
        ui->payAmount->setValid(false);
        retval = false;
    }

    // Reject dust outputs:
    if (retval && GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        retval = false;
    }

    return retval;
}

SendCoinsRecipient SendCoinsEntry::getValue()
{
    // Payment request
    if (recipient.paymentRequest.IsInitialized())
        return recipient;

    // Normal payment
    recipient.address = ui->payTo->text();
    recipient.label = ui->addAsLabel->text();
    recipient.amount = ui->payAmount->value();
    recipient.message = ui->messageTextLabel->text();
    recipient.fSubtractFeeFromAmount = (ui->checkboxSubtractFeeFromAmount->checkState() == Qt::Checked);
	recipient.fForce = (ui->chkForceTithe->checkState() == Qt::Checked);
	recipient.fTithe = (ui->checkboxTithe->checkState() == Qt::Checked);
	recipient.fPrayer = (ui->checkboxPrayer->checkState() == Qt::Checked);
	recipient.fRepent = (ui->checkboxRepent->checkState() == Qt::Checked);
	recipient.txtMessage = ui->txtMessage->text();
	recipient.txtRepent = ui->comboRepent->currentText();
    recipient.ipfshash = ui->txtFile->text();
    return recipient;
}

QWidget *SendCoinsEntry::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, ui->payTo);
    QWidget::setTabOrder(ui->payTo, ui->addAsLabel);
    QWidget *w = ui->payAmount->setupTabChain(ui->addAsLabel);
    QWidget::setTabOrder(w, ui->checkboxSubtractFeeFromAmount);
    QWidget::setTabOrder(ui->checkboxSubtractFeeFromAmount, ui->addressBookButton);
    QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    return ui->deleteButton;
}

void SendCoinsEntry::setValue(const SendCoinsRecipient &value)
{
    recipient = value;

    if (recipient.paymentRequest.IsInitialized()) // payment request
    {
        if (recipient.authenticatedMerchant.isEmpty()) // unauthenticated
        {
            ui->payTo_is->setText(recipient.address);
            ui->memoTextLabel_is->setText(recipient.message);
            ui->payAmount_is->setValue(recipient.amount);
            ui->payAmount_is->setReadOnly(true);
			ui->txtMessage->setText(recipient.txtMessage);
			
            setCurrentWidget(ui->SendCoins_UnauthenticatedPaymentRequest);
        }
        else // authenticated
        {
            ui->payTo_s->setText(recipient.authenticatedMerchant);
            ui->memoTextLabel_s->setText(recipient.message);
            ui->payAmount_s->setValue(recipient.amount);
            ui->payAmount_s->setReadOnly(true);
			ui->txtMessage->setText(recipient.txtMessage);
            setCurrentWidget(ui->SendCoins_AuthenticatedPaymentRequest);
        }
    }
    else // normal payment
    {
        // message
        ui->messageTextLabel->setText(recipient.message);
        ui->messageTextLabel->setVisible(!recipient.message.isEmpty());
		ui->txtMessage->setText(recipient.txtMessage);
        ui->messageLabel->setVisible(!recipient.message.isEmpty());
        ui->addAsLabel->clear();
		initPOGDifficulty();

        ui->payTo->setText(recipient.address); // this may set a label from addressbook
        if (!recipient.label.isEmpty()) // if a label had been set from the addressbook, don't overwrite with an empty label
            ui->addAsLabel->setText(recipient.label);
        ui->payAmount->setValue(recipient.amount);
    }
}

void SendCoinsEntry::setAddress(const QString &address)
{
    ui->payTo->setText(address);
    ui->payAmount->setFocus();
}

bool SendCoinsEntry::isClear()
{
    return ui->payTo->text().isEmpty() && ui->payTo_is->text().isEmpty() && ui->payTo_s->text().isEmpty();
}

void SendCoinsEntry::setFocus()
{
    ui->payTo->setFocus();
}

void SendCoinsEntry::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        // Update payAmount with the current unit
        ui->payAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_is->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_s->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    }
}

bool SendCoinsEntry::updateLabel(const QString &address)
{
    if(!model)
        return false;

    // Fill in label from address book, if address has an associated label
    QString associatedLabel = model->getAddressTableModel()->labelForAddress(address);
    if(!associatedLabel.isEmpty())
    {
        ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}
