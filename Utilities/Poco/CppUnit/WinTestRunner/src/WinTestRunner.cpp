//
// WinTestRunner.cpp
//
// $Id: //poco/1.3/CppUnit/WinTestRunner/src/WinTestRunner.cpp#2 $
//


#include "WinTestRunner/WinTestRunner.h"
#include "TestRunnerDlg.h"
#include "CppUnit/TestRunner.h"
#include <fstream>


namespace CppUnit {


WinTestRunner::WinTestRunner()
{
}


WinTestRunner::~WinTestRunner()
{
	for (std::vector<Test*>::iterator it = _tests.begin(); it != _tests.end(); ++it)
		delete *it;
}


void WinTestRunner::run()
{
	// Note: The following code is some evil hack to
	// add batch capability to the MFC based WinTestRunner.
	
	std::string cmdLine(AfxGetApp()->m_lpCmdLine);
	if (cmdLine.size() >= 2 && cmdLine[0] == '/' && (cmdLine[1] == 'b' || cmdLine[1] == 'B'))
	{
		// We're running in batch mode.
		std::string outPath;
		if (cmdLine.size() > 4 && cmdLine[2] == ':')
			outPath = cmdLine.substr(3);
		else
			outPath = "CON";
		std::ofstream ostr(outPath.c_str());
		if (ostr.good())
		{
			TestRunner runner(ostr);
			for (std::vector<Test*>::iterator it = _tests.begin(); it != _tests.end(); ++it)
				runner.addTest((*it)->toString(), *it);
			_tests.clear();
			std::vector<std::string> args;
			args.push_back("WinTestRunner");
			args.push_back("-all");
			bool success = runner.run(args);
			ExitProcess(success ? 0 : 1);
		}
		else ExitProcess(2);
	}
	else
	{
		// We're running in interactive mode.
		TestRunnerDlg dlg;
		dlg.setTests(_tests);
		dlg.DoModal();
	}
}


void WinTestRunner::addTest(Test* pTest)
{
	_tests.push_back(pTest);
}


BEGIN_MESSAGE_MAP(WinTestRunnerApp, CWinApp)
END_MESSAGE_MAP()


BOOL WinTestRunnerApp::InitInstance()
{	
	AllocConsole();
	SetConsoleTitle("CppUnit WinTestRunner Console");
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	freopen("CONIN$", "r", stdin);
	TestMain();
	FreeConsole();
	return FALSE;
}


void WinTestRunnerApp::TestMain()
{
}


} // namespace CppUnit
