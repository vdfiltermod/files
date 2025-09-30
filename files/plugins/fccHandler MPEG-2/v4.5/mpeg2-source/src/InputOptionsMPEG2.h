#ifndef _INPUTOPTIONSMPEG2_H_
#define _INPUTOPTIONSMPEG2_H_

class InputOptionsMPEG2 : public vdxunknown<IVDXInputOptions> {
private:
	static  INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

public:
	InputOptionsMPEG2();

	// IVDXInputOptions
	uint32  VDXAPIENTRY Write(void *buf, uint32 buflen);

	void    OptionsDlg(HWND hwndParent);
	static  void InitDialogTitle(HWND hDlg, char *pTitle);

	struct  OptionsMPEG2 {
		uint32  len;
		bool    fMultipleOpen;
		bool    fCreateIndex;
		bool    fAllowDSC;
		bool    fAllowMatrix;
	} opts;
};

#endif  // _INPUTOPTIONSMPEG2_H_