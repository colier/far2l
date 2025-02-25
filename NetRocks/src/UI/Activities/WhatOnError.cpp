#include <utils.h>
#include "WhatOnError.h"
#include "../../Globals.h"



/*                                                         62
345              20                                          64
 ====================== Download error ======================
| [EDITBOX                                                 ] |
| Object:        [EDITBOX                                  ] |
| Site:          [EDITBOX                                  ] |
|------------------------------------------------------------|
| [ ] Re&member my choice for this operation                 |
|------------------------------------------------------------|
|   [ &Retry ]  [ &Skip ]         [        &Cancel       ]   |
 ============================================================
    6                     29       38                      60
*/

WhatOnError::WhatOnError(WhatOnErrorKind wek, const std::string &error, const std::string &object, const std::string &site)
{
	int title_lng;
	switch (wek) {
		case WEK_DOWNLOAD:  title_lng = MErrorDownloadTitle; break;
		case WEK_UPLOAD:    title_lng = MErrorUploadTitle; break;
		case WEK_CROSSLOAD: title_lng = MErrorCrossloadTitle; break;
		case WEK_QUERYINFO: title_lng = MErrorQueryInfoTitle; break;
		case WEK_ENUMDIR:   title_lng = MErrorEnumDirTitle; break;
		case WEK_MAKEDIR:   title_lng = MErrorMakeDirTitle; break;
		case WEK_RENAME:    title_lng = MErrorRenameTitle; break;
		case WEK_REMOVE:    title_lng = MErrorRemoveTitle; break;
		case WEK_SETTIMES:  title_lng = MErrorSetTimes; break;
		case WEK_CHMODE:    title_lng = MErrorChangeMode; break;
		case WEK_SYMLINK_QUERY:     title_lng = MErrorSymlinkQuery; break;
		case WEK_SYMLINK_CREATE:     title_lng = MErrorSymlinkCreate; break;

		default: {
			fprintf(stderr, "Error: wrong kind %u\n", wek);
			abort();
		}
	}

	_di.Add(DI_DOUBLEBOX, 3,1,64,9, 0, title_lng);

	_di.SetLine(2);
//	_di.AddAtLine(DI_TEXT, 5,19, 0, MErrorError);
	_di.AddAtLine(DI_TEXT, 5,62, 0, error.c_str());

	_di.NextLine();
	_di.AddAtLine(DI_TEXT, 5,19, 0, MErrorObject);
	_di.AddAtLine(DI_TEXT, 20,62, 0, object.c_str());

	_di.NextLine();
	_di.AddAtLine(DI_TEXT, 5,19, 0, MErrorSite);
	_di.AddAtLine(DI_TEXT, 20,62, 0, site.c_str());

	_di.NextLine();
	_di.AddAtLine(DI_TEXT, 4,63, DIF_BOXCOLOR | DIF_SEPARATOR);

	_di.NextLine();
	_i_remember = _di.AddAtLine(DI_CHECKBOX, 5,33, 0, MRememberChoice);

	_di.NextLine();
	_di.AddAtLine(DI_TEXT, 4,63, DIF_BOXCOLOR | DIF_SEPARATOR);

	_di.NextLine();
	_i_retry = _di.AddAtLine(DI_BUTTON, 5,20, DIF_CENTERGROUP, MRetry);
	_i_skip = _di.AddAtLine(DI_BUTTON, 21,40, DIF_CENTERGROUP, MSkip);
	_di.AddAtLine(DI_BUTTON, 41,60, DIF_CENTERGROUP, MCancel);

	SetFocusedDialogControl(_i_retry);
	SetDefaultDialogControl(_i_retry);

	G.info.FSF->DisplayNotification(L"far2l - NetRocks", G.GetMsgWide(title_lng));

}


WhatOnErrorAction WhatOnError::Ask(WhatOnErrorAction &default_wea)
{
	int r = Show(L"WhatOnError", 6, 2, FDLG_WARNING);

	WhatOnErrorAction out = WEA_CANCEL;

	if (r == _i_retry) {
		out = WEA_RETRY;

	} else if (r == _i_skip) {
		out = WEA_SKIP;
	}

	if (out != WEA_CANCEL && IsCheckedDialogControl(_i_remember)) {
		default_wea = out;
	}

	return out;
}

//////////////////////////////////////


WhatOnErrorAction WhatOnErrorState::Query(ProgressState &progress_state, WhatOnErrorKind wek, const std::string &error, const std::string &object, const std::string &site)
{
	auto i = _default_weas[wek].emplace(error, WEA_ASK).first;
	if (i->second != WEA_ASK) {
		if (i->second == WEA_RETRY) {
			for (unsigned int sleep_usec = _auto_retry_delay * 1000000; sleep_usec; ) {
				unsigned int sleep_usec_portion = (sleep_usec > 100000) ? 100000 : sleep_usec;
				usleep(sleep_usec_portion);
				sleep_usec-= sleep_usec_portion;
				std::lock_guard<std::mutex> locker(progress_state.mtx);
				if (progress_state.aborting) {
					return WEA_CANCEL;
				}
			}
			if (_auto_retry_delay < 5) {
				++_auto_retry_delay;
			}
		}

		return i->second;
	}

	++_showing_ui;
	auto out = WhatOnError(wek, error, object, site).Ask(i->second);
	--_showing_ui;

	return out;
}

void WhatOnErrorState::ResetAutoRetryDelay()
{
	_auto_retry_delay = 0;
}

void WhatOnErrorWrap_DummyOnRetry()
{
}


