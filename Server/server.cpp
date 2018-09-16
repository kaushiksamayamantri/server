#include "tchar.h"
#include <io.h>
#include <fcntl.h>
#include "ipc_server.h"

int _tmain(int argc, _TCHAR* argv[])
{
	LOG << _T("-------------------------IPC SERVER---------------------------") << std::endl;
	std::wstring sPipeName(PIPENAME);
	CipcServer* pServer = new CipcServer(sPipeName);
	::WaitForSingleObject(pServer->GetThreadHandle(), INFINITE);
	delete pServer;
	pServer = NULL;
	return 0;
}

