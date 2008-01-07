/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

// @todo handle raw 433 (nick in use)
#include "IRCSock.h"
#include "User.h"

class CAwayNickMod;

class CAwayNickTimer : public CTimer {
public:
	CAwayNickTimer(CAwayNickMod& Module);

private:
	virtual void RunJob();

private:
	CAwayNickMod&	m_Module;
};

class CBackNickTimer : public CTimer {
public:
	CBackNickTimer(CModule& Module)
		: CTimer(&Module, 3, 1, "BackNickTimer", "Set your nick back when you reattach"),
		  m_Module(Module) {}

private:
	virtual void RunJob() {
		CUser* pUser = m_Module.GetUser();

		if (pUser->IsUserAttached() && pUser->IsIRCConnected()) {
			CString sConfNick = pUser->GetNick();
			m_Module.PutIRC("NICK " + sConfNick);
		}
	}

private:
	CModule&	m_Module;
};

class CAwayNickMod : public CModule {
public:
	MODCONSTRUCTOR(CAwayNickMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!sArgs.empty())
			m_sFormat = sArgs;
		else
			m_sFormat = GetNV("nick");

		if (m_sFormat.empty()) {
			m_sFormat = "zz_%nick%";
		}

		SetNV("nick", m_sFormat);

		if (m_pUser->GetKeepNick()) {
			sMessage = "You have KeepNick enabled. "
				"This won't work together with awaynick.";
			return false;
		}

		return true;
	}

	virtual ~CAwayNickMod() {
	}

	void StartAwayNickTimer() {
		RemTimer("AwayNickTimer");
		AddTimer(new CAwayNickTimer(*this));
	}

	void StartBackNickTimer() {
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock) {
			CString sConfNick = m_pUser->GetNick();

			if (pIRCSock->GetNick().CaseCmp(GetAwayNick().Left(pIRCSock->GetNick().length())) == 0) {
				RemTimer("BackNickTimer");
				AddTimer(new CBackNickTimer(*this));
			}
		}
	}

	virtual void OnIRCConnected() {
		if (m_pUser && !m_pUser->IsUserAttached()) {
			StartAwayNickTimer();
		}
	}

	virtual void OnIRCDisconnected() {
		RemTimer("AwayNickTimer");
		RemTimer("BackNickTimer");
	}

	virtual void OnUserAttached() {
		if (m_pUser->GetKeepNick()) {
			PutModule("WARNING: You have KeepNick enabled. "
					"This won't work with awaynick.");
		}

		StartBackNickTimer();
	}

	virtual void OnUserDetached() {
		if (!m_pUser->IsUserAttached()) {
			StartAwayNickTimer();
		}
	}

	virtual void OnModCommand(const CString& sCommand) {
		if (strcasecmp(sCommand.c_str(), "TIMERS") == 0) {
			ListTimers();
		} else if (sCommand.Token(0).CaseCmp("SET") == 0) {
			CString sFormat(sCommand.Token(1));

			if (!sFormat.empty()) {
				m_sFormat = sFormat;
				SetNV("nick", m_sFormat);
			}

			if (m_pUser) {
				CString sExpanded = GetAwayNick();
				CString sMsg = "AwayNick is set to [" + m_sFormat + "]";

				if (m_sFormat != sExpanded) {
					sMsg += " (" + sExpanded + ")";
				}

				PutModule(sMsg);
			}
		} else if (sCommand.Token(0).CaseCmp("SHOW") == 0) {
			if (m_pUser) {
				CString sExpanded = GetAwayNick();
				CString sMsg = "AwayNick is set to [" + m_sFormat + "]";

				if (m_sFormat != sExpanded) {
					sMsg += " (" + sExpanded + ")";
				}

				PutModule(sMsg);
			}
		} else if(sCommand.Token(0).CaseCmp("HELP") == 0) {
			PutModule("Commands are: show, timers, set [awaynick]");
		}
	}

	CString GetAwayNick() {
		unsigned int uLen = 9;
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock) {
			uLen = pIRCSock->GetMaxNickLen();
		}

		return m_pUser->ExpandString(m_sFormat).Left(uLen);
	}

private:
	CString		m_sFormat;
};

CAwayNickTimer::CAwayNickTimer(CAwayNickMod& Module)
	: CTimer(&Module, 30, 1, "AwayNickTimer", "Set your nick while you're detached"),
	  m_Module(Module) {}

void CAwayNickTimer::RunJob() {
	CUser* pUser = m_Module.GetUser();

	if (!pUser->IsUserAttached() && pUser->IsIRCConnected()) {
		m_Module.PutIRC("NICK " + m_Module.GetAwayNick());
	}
}

MODULEDEFS(CAwayNickMod, "Change your nick while you are away")
