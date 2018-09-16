#include "process.h"
#include "windows.h"
#include "tchar.h"
#include "ipc_server.h"


CipcServer::CipcServer(void)
{
}

CipcServer::CipcServer(std::wstring& sName) : mPipeName(sName), mThread(NULL), mEvent(MS_INIT)
{
	mbuffer = (wchar_t*)calloc(MS_DATA_BUF, sizeof(wchar_t));
	Init();
}

CipcServer::~CipcServer(void)
{
	delete mbuffer;
	mbuffer = NULL;
}

int CipcServer::GetEvent() const
{
	return mEvent;
}

void CipcServer::SetEvent(int nEventID)
{
	mEvent = nEventID;
}

HANDLE CipcServer::GetThreadHandle()
{
	return mThread;
}

HANDLE CipcServer::GetPipeHandle()
{
	return mPipe;
}

void CipcServer::SetData(std::wstring& sData)
{
	memset(&mbuffer[0], 0, MS_DATA_BUF);
	wcsncpy(&mbuffer[0], sData.c_str(), __min(MS_DATA_BUF, sData.size()));
}

void CipcServer::GetData(std::wstring& sData)
{
	sData.clear();
	sData.append(mbuffer);
}

void CipcServer::Init()
{
	if (mPipeName.empty())
	{
		LOG << "<<<<   Error: Invalid pipe name" << std::endl;
		return;
	}

	mPipe = ::CreateNamedPipe(mPipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
							  MS_DATA_BUF, MS_DATA_BUF, NMPWAIT_USE_DEFAULT_WAIT, NULL);

	if (INVALID_HANDLE_VALUE == mPipe)
	{
		LOG << "<<<<   Error: Could not create named pipe" << std::endl;
		OnEvent(MS_ERROR);
	}
	else
		OnEvent(MS_SERVER_RUN);

	Run();
}

void CipcServer::Run()
{
	UINT threadID = 0;
	mThread = (HANDLE)_beginthreadex(NULL, NULL, PipeCallback, this, CREATE_SUSPENDED, &threadID);

	if (NULL == mThread)
		OnEvent(MS_ERROR);
	else
	{
		SetEvent(MS_INIT);
		::ResumeThread(mThread);
	}
}

UINT32 __stdcall CipcServer::PipeCallback(void* pParam)
{
	CipcServer* pPipe = reinterpret_cast<CipcServer*>(pParam);
	if (pPipe == NULL)
		return 1L;

	pPipe->OnEvent(MS_THREAD_RUN);
	while (true)
	{
		int eventID = pPipe->GetEvent();
		if (eventID == MS_ERROR || eventID == MS_TERMINATE)
		{
			pPipe->Close();
			break;
		}

		switch (eventID)
		{
		case MS_INIT:
		{
			pPipe->WaitForClient();
			break;
		}

		case MS_IOREAD:
		{
			if (pPipe->Read())
				pPipe->OnEvent(MS_READ);
			else
				pPipe->OnEvent(MS_ERROR);

			break;
		}

		case MS_IOWRITE:
		{
			if (pPipe->Write())
				pPipe->OnEvent(MS_WRITE);
			else
				pPipe->OnEvent(MS_ERROR);
		}
		break;

		case MS_CLOSE:
		{
			pPipe->OnEvent(MS_CLOSE);
			break;
		}

		case MS_IOWRITECLOSE:
		{
			if (pPipe->Write())
				pPipe->OnEvent(MS_CLOSE);
			else
				pPipe->OnEvent(MS_ERROR);

			break;
		}

		case MS_IOPENDING:
		default:
			Sleep(10);
			continue;
		};

		Sleep(10);
	};

	return 0;
}

void CipcServer::OnEvent(int nEventID)
{
	switch (nEventID)
	{
	case MS_THREAD_RUN:
		LOG << "<<<<   Thread is running" << std::endl;
		break;

	case MS_INIT:
		LOG << "<<<<   Initializing IPC communication" << std::endl;
		break;

	case MS_SERVER_RUN:
		LOG << "<<<<   IPC server running" << std::endl;
		break;

	case MS_CLIENT_WAIT:
		LOG << "<<<<   Waiting for IPC client" << std::endl;
		break;

	case MS_CLIENT_CONN:
	{
		LOG << "<<<<   Enter Data to be communciated to client" << std::endl;
		std::wstring sData;
		std::getline(std::wcin, sData);
		SetData(sData);
		if (wcscmp(sData.c_str(), _T("close")) == 0)
			SetEvent(MS_IOWRITECLOSE);
		else
			SetEvent(MS_IOWRITE);
		break;
	}

	case MS_READ:
	{
		std::wstring sData;
		GetData(sData);
		LOG << "Message from client: " << sData << std::endl;
		LOG << "<<<<   If message recieved close Application will be closed " << std::endl;

		if (wcscmp(sData.c_str(), _T("close")) == 0)
			SetEvent(MS_CLOSE);
		else
		{
			sData.clear();
			LOG << "<<<<   Enter Data to be communciated to server" << std::endl;
			std::getline(std::wcin, sData);
			SetData(sData);
			if (wcscmp(sData.c_str(), _T("close")) == 0)
				SetEvent(MS_IOWRITECLOSE);
			else
				SetEvent(MS_IOWRITE);
		}
		
		break;
	}
	case MS_WRITE:
		LOG << "<<<<   Wrote data to pipe" << std::endl;
		SetEvent(MS_IOREAD);
		break;

	case MS_ERROR:
		LOG << "<<<<   ERROR: Pipe error" << std::endl;
		SetEvent(MS_ERROR);
		break;

	case MS_CLOSE:
		LOG << "<<<<   Closing pipe" << std::endl;
		SetEvent(MS_TERMINATE);
		break;
	};
}

void CipcServer::WaitForClient()
{
	OnEvent(MS_CLIENT_WAIT);
	if (FALSE == ConnectNamedPipe(mPipe, NULL)) 
		OnEvent(MS_ERROR);
	else
		OnEvent(MS_CLIENT_CONN);
}

void CipcServer::Close()
{
	::CloseHandle(mPipe);
	mPipe = NULL;
}

bool CipcServer::Read()
{
	DWORD bytes = 0;
	BOOL status = FALSE;
	int read_bytes = 0;
	do
	{
		status = ::ReadFile(mPipe, &mbuffer[read_bytes], MS_DATA_BUF, &bytes, NULL);

		if (!status && ERROR_MORE_DATA != GetLastError())
		{
			status = FALSE;
			break;
		}
		read_bytes += bytes;

	} while (!status);

	if (FALSE == status || 0 == bytes)
		return false;

	return true;
}

bool CipcServer::Write()
{
	DWORD bytes;
	BOOL status = ::WriteFile(mPipe, mbuffer, ::wcslen(mbuffer)*sizeof(wchar_t) + 1, &bytes, NULL);
	if (FALSE == status || wcslen(mbuffer)*sizeof(wchar_t) + 1 != bytes)
		return false;
	return true;
}
