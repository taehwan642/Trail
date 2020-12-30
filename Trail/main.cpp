#pragma region HEADERS
#include <Windows.h>
#include <iostream>
#include <mmsystem.h>
#include <d3dx9.h>
#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )
#include <vector>
#pragma endregion
using namespace std;

LPDIRECT3D9             g_pD3D = NULL; // 디바이스를 초기하기 위해서 사용하는 구조체
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL; // 사용할 디바이스
D3DXVECTOR3 trailuppos = { 0, 0.5f, 0 };
D3DXVECTOR3 traildownpos = { 0, -0.5f, 0 };
float deltatime = 0; // 놀랍게도 이거 야매
LPD3DXMESH box;
D3DXVECTOR3 boxpos;
D3DXVECTOR3 boxrot;
D3DXVECTOR3 boxmin, boxmax;
D3DXMATRIX boxWorld;

struct VTXCOL
{
	D3DXVECTOR3 pos;
	DWORD color;
};

#define VTXCOL_FVF (D3DFVF_XYZ|D3DFVF_DIFFUSE)

struct INDEX16
{
	unsigned short _0;
	unsigned short _1;
	unsigned short _2;
};

class TrailEffect
{
private:
	struct TrailData // 각 트레일이 가지고 있을 데이터
	{
		D3DXVECTOR3 position[2];
		double timecount = 0.0;
		TrailData(const D3DXVECTOR3& upposition, const D3DXVECTOR3& downposition)
			: timecount(0.0)
		{
			position[0] = upposition;
			position[1] = downposition;
		}

	};

private:
	LPDIRECT3DDEVICE9 device = nullptr; // 트레일을 그릴 디바이스
	unsigned long vtxSize = 0; // 정점 크기 (sizeof(VTXCOL))
	unsigned long maxvtxCnt = 0; // 최대 정점 개수
	unsigned long maxtriCnt = 0; // 최대 삼각형 개수
	unsigned long curTriCnt = 0;  // 현재 삼각형 개수
	unsigned long curVtxCnt = 0; // 현재 정점 개수
	double duration = 0.0; // 트레일이 살아있을 수 있는 최대 시간
	double alivetime = 0.0;
	size_t lerpcnt = 0; // 와 ciba! 이건 i / lerpcnt d (기본 일단 10개로 세팅)
	float timer = 0.0f;
	std::vector<TrailData> trailDatas; // 트레일들을 담을 컨테이너



public:
	bool renderline = false;
	explicit TrailEffect() {};
	virtual ~TrailEffect() {};
	LPDIRECT3DVERTEXBUFFER9 vb = nullptr; // 버텍스 버퍼
	LPDIRECT3DINDEXBUFFER9  ib = nullptr; // 인덱스 버퍼

public:
	HRESULT Initalize(LPDIRECT3DDEVICE9& dev, const unsigned long& _bufferSize, const unsigned long& _lerpCnt, const  double& _duration, const double& _alivetime, const size_t& _lerpcnt);
	void AddNewTrail(const D3DXVECTOR3& upposition, const D3DXVECTOR3& downposition);
	void SplineTrailPosition(VTXCOL* vtx, const size_t& dataindex, unsigned long& index); // vertexindex는 updateTrail 속 for문에서 넣는거고, index는 그 maxVtxCnt 비교문 해야함
	void UpdateTrail();
	void RenderTrail();
};

HRESULT TrailEffect::Initalize(LPDIRECT3DDEVICE9& dev, const unsigned long& _bufferSize, const unsigned long& _lerpCnt, const double& _duration, const double& _alivetime, const size_t& _lerpcnt)
{
	HRESULT hr = S_OK;

	device = dev;

	maxvtxCnt = _bufferSize;
	if (maxvtxCnt <= 2) // 최소 1개의 삼각형이 만들어질 수 없는 환경이 세팅이 되지 못했다면
		goto error;

	maxtriCnt = maxvtxCnt - 2;
	vtxSize = sizeof(VTXCOL);
	duration = _duration;
	alivetime = _alivetime;
	lerpcnt = _lerpcnt;

	// 정점 버퍼 생성
	if (FAILED(device->CreateVertexBuffer(maxvtxCnt * vtxSize, 0, VTXCOL_FVF, D3DPOOL_MANAGED, &vb, nullptr)))
		goto error;

	// 인덱스 버퍼 생성
	if (FAILED(device->CreateIndexBuffer(sizeof(INDEX16) * maxtriCnt, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib, nullptr)))
		goto error;
	
	return hr;

error:
	hr = E_FAIL;
	return hr;
}

void TrailEffect::AddNewTrail(const D3DXVECTOR3& upposition, const D3DXVECTOR3& downposition)
{
	timer += deltatime;
	if (duration < (double)timer)
	{
		TrailData data(upposition, downposition);
		trailDatas.emplace_back(data);
		timer = 0;
	}
}

void TrailEffect::SplineTrailPosition(VTXCOL* vtx, const size_t& dataindex, unsigned long& index)
{
	D3DXMATRIX im;
	D3DXMatrixInverse(&im, 0, &boxWorld);

	if (maxvtxCnt <= index)
		return;

	size_t iCurIndex = index;

	D3DXVec3TransformCoord(&vtx[index].pos, &trailDatas[dataindex].position[0], &im);
	vtx[index++].color = D3DCOLOR_XRGB(255, 0, 204);

	if (maxvtxCnt <= index)
		return;
	D3DXVec3TransformCoord(&vtx[index].pos, &trailDatas[dataindex].position[1], &im);
	vtx[index++].color = D3DCOLOR_XRGB(255, 255, 0);

	if (maxvtxCnt <= index)
		return;

	D3DXVECTOR3 vLerpPos[2];

	unsigned long iSize = trailDatas.size();

	// 보간 
	for (unsigned long j = 1 /*분모가 0이면 BOOM!*/; j < lerpcnt; ++j)
	{
		int iEditIndexV0 = (dataindex < 1 ? 0 : dataindex - 1);
		int iEditIndexV2 = (dataindex + 1 >= iSize ? dataindex : dataindex + 1);
		int iEditIndexV3 = (dataindex + 2 >= iSize ? iEditIndexV2 : dataindex + 2);

		D3DXVec3CatmullRom(&vLerpPos[0],
			&trailDatas[iEditIndexV0].position[0],
			&trailDatas[dataindex].position[0],
			&trailDatas[iEditIndexV2].position[0],
			&trailDatas[iEditIndexV3].position[0],
			j / float(lerpcnt));
		
		D3DXVec3CatmullRom(&vLerpPos[1],
			&trailDatas[iEditIndexV0].position[1],
			&trailDatas[dataindex].position[1],
			&trailDatas[iEditIndexV2].position[1],
			&trailDatas[iEditIndexV3].position[1],
			j / float(lerpcnt));


		D3DXVec3TransformCoord(&vtx[index].pos, &vLerpPos[0], &im);
		vtx[index].color = D3DCOLOR_XRGB(255, 0, 204);
		++index;

		if (maxvtxCnt <= index)
			return;

		D3DXVec3TransformCoord(&vtx[index].pos, &vLerpPos[1], &im);
		vtx[index].color = D3DCOLOR_XRGB(255, 255, 0);
		++index;
		if (maxvtxCnt <= index)
			return;
	}
}

void TrailEffect::UpdateTrail()
{
	// 트레일 데이터 업데이트 부분 //
	auto iterEnd = trailDatas.end(); // end까지 받고
	for (auto iter = trailDatas.begin(); iter != trailDatas.end(); ) // 순회
	{
		iter->timecount += deltatime; // ㅆㅣ발 deltatime 받고
		// 지속 시간이 지났다면
		if (iter->timecount >= alivetime)
			iter = trailDatas.erase(iter);
		else
			++iter;
	}
	////////////////////////////////////////

	if (trailDatas.size() <= 1) // 정점이 2개밖에 나오지 않으니, 삼각형이 만들어지지 못하니 그냥 리턴
		return;
	VTXCOL* pVertex = nullptr;
	INDEX16* pIndex = nullptr;

	vb->Lock(0, 0, reinterpret_cast<void**>(&pVertex), 0);
	ib->Lock(0, 0, reinterpret_cast<void**>(&pIndex), 0);

	std::size_t dataCnt = trailDatas.size();
	unsigned long index = 0;

	// 정점 위치, uv 설정

	for (std::size_t i = 0; i < dataCnt; ++i)
	{
		/*D3DXVec3TransformCoord(&pVertex[index].pos, &trailDatas[i].position[0], &im);
		pVertex[index++].color = D3DCOLOR_XRGB(255, 0, 204);
		D3DXVec3TransformCoord(&pVertex[index].pos, &trailDatas[i].position[1], &im);
		pVertex[index++].color = D3DCOLOR_XRGB(255, 255, 0);*/

		//pVertex[index].pos = trailDatas[i].position[0];
		//pVertex[index].pos = trailDatas[i].position[1];
		SplineTrailPosition(pVertex, i, index);

		// 로컬 행렬로 변환해줘야함. 

		// 오버플로우 방지 ㅇㅇ
		if (maxvtxCnt <= index)
			break;
	}



	// 인덱스 버퍼 설정
	curVtxCnt = index; // 위에서 결정된 최종 그려질 정점의 개수 
	curTriCnt = curVtxCnt - 2;
	for (unsigned long i = 0; i < curTriCnt; i+=2)
	{
		// tricnt에서 문제가 있다!
		pIndex[i]._0 = i;
		pIndex[i]._1 = i + 1;
		pIndex[i]._2 = i + 3;
		pIndex[i + 1]._0 = i;
		pIndex[i + 1]._1 = i + 3;
		pIndex[i + 1]._2 = i + 2;
	}

	vb->Unlock();
	ib->Unlock();
}

void TrailEffect::RenderTrail()
{
	if (trailDatas.size() <= 1)
		return;

	device->SetStreamSource(0, vb, 0, vtxSize); // 정점 버퍼 연결
	device->SetFVF(VTXCOL_FVF);
	device->SetIndices(ib);
	if (renderline)
		device->DrawIndexedPrimitive(D3DPT_LINELIST, 0, 0, curVtxCnt, 0, curTriCnt); // 현재 결정된 정점 개수와 삼각형 개수만큼 그린다.
	else
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, curVtxCnt, 0, curTriCnt); // 현재 결정된 정점 개수와 삼각형 개수만큼 그린다.

}

TrailEffect* trail; // 트레일을 관리해주는 클래스

// psudocode
// 버텍스 버퍼, 인덱스 버퍼 초기화 OK
// 키를 누르면 실시간으로 추가되는지 확인 OK
// 일단, 키를 누르면 트레일을 특정 위치 += ? 에 추가시켜, 삼각형이 실시간으로 그려지는지 확인하기 OK
// 동적으로 버퍼 껴주기 OK
// 위치 변하면서 계속 그려지는지 확인 OK
// 칼 크기의 Box의 Up, Down 로컬 위치에 부착시켜주기. OK
// 부착 뒤 박스의 Z Rotation을 돌려주기
// 돌아가면서 보간이 잘 되는지 확인
// 버텍스 반대편부터 그려야하는지 확인
// 그려주기



HRESULT InitD3D(HWND hWnd)
{
	if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
		return E_FAIL;

	// Set up the structure used to create the D3DDevice
	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

	// Create the D3DDevice
	if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&d3dpp, &g_pd3dDevice)))
		return E_FAIL;

	g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	trail = new TrailEffect();

	return S_OK;
}

HRESULT InitGeometry()
{

	if (FAILED(D3DXCreateBox(g_pd3dDevice, 0.5, 2, 0.5, &box, nullptr)))
		return E_FAIL;
	
	D3DXVECTOR3* pVertices;
	
	if (FAILED(box->LockVertexBuffer(D3DLOCK_READONLY, reinterpret_cast<void**>(&pVertices))))
		return E_FAIL;

	if (FAILED(D3DXComputeBoundingBox(pVertices, box->GetNumVertices(), box->GetNumBytesPerVertex(), &boxmin, &boxmax)))
		return E_FAIL;
	
	box->UnlockVertexBuffer();

	// 씨빨 버그 뭐야

	if (SUCCEEDED(trail->Initalize(g_pd3dDevice, 1000, 0, 0.03f, 1, 20)))
		cout << "트레일 초기화 완료" << endl;




	return S_OK;
}


VOID PipelineSetup()
{
	D3DXMATRIXA16 matWorld;
	//D3DXMatrixIdentity(&matWorld);
	//UINT iTime = timeGetTime() % 1000;
	//float t = (float)iTime / 1000;
	deltatime = 0.015f;
	//FLOAT fAngle = (iTime * (2.0f * D3DX_PI) / 1000.0f);
	//D3DXMatrixRotationY(&matWorld, fAngle);
	D3DXMATRIX matRot; // 씨발ㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋ이실수를하네
	D3DXMatrixRotationYawPitchRoll(&matRot, boxrot.y, boxrot.x, boxrot.z);
	D3DXMatrixTranslation(&matWorld, boxpos.x, boxpos.y, boxpos.z);

	matRot *= matWorld;

	boxWorld = matRot;

	g_pd3dDevice->SetTransform(D3DTS_WORLD, &matRot);

	D3DXVECTOR3 vEyePt(0.0f, 3.0f, -5.0f);
	D3DXVECTOR3 vLookatPt(0.0f, 0.0f, 0.0f);
	D3DXVECTOR3 vUpVec(0.0f, 1.0f, 0.0f);
	D3DXMATRIXA16 matView;
	D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);
	g_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);

	D3DXMATRIXA16 matProj;
	D3DVIEWPORT9 viewPort;
	g_pd3dDevice->GetViewport(&viewPort);
	D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian(60.f), viewPort.Width / (float)viewPort.Height, 1.f, 500.f);
	g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
}


bool ispressed[3];

void CheckKeyInput()
{
	if (GetAsyncKeyState('W') & 0x8000)
	{
		boxpos.z += 0.1f;
	}

	if (GetAsyncKeyState('S') & 0x8000)
	{
		boxpos.z -= 0.1f;
	}

	if (GetAsyncKeyState('A') & 0x8000)
	{
		boxpos.x -= 0.1f;
	}

	if (GetAsyncKeyState('D') & 0x8000)
	{
		boxpos.x += 0.1f;
	}

	if (GetAsyncKeyState('E') & 0x8000)
	{
		boxrot.z -= 0.3f;
	}
	if (GetAsyncKeyState('Q') & 0x8000)
	{
		boxrot.z += 0.3f;
	}

	if ((GetAsyncKeyState('O') & 0x8000))
	{
		if (!ispressed[1])
		{

			ispressed[1] = true;
			trail->renderline = !trail->renderline;
			// 기준위치를 중심으로 조금씩 움직이는 모습이 가장 예쁠듯.
		}
	}
	else
	{
		ispressed[1] = false;
	}
	

}

VOID Render()
{
	// Clear the backbuffer to a black color
	g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

	// Begin the scene
	if (SUCCEEDED(g_pd3dDevice->BeginScene()))
	{
		CheckKeyInput();
		PipelineSetup();


		D3DXVECTOR3 vUp = *reinterpret_cast<D3DXVECTOR3*>(&boxWorld._21);
		
		D3DXVec3Normalize(&vUp, &vUp);
		

		//g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		//g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

		D3DXVECTOR3 vDownPos = boxpos + vUp * -1.f;
		D3DXVECTOR3 vUpPos = boxpos + vUp * 1.f;




		trail->AddNewTrail(vDownPos, vUpPos); // 위치정보 넣어줘야함!
		//trail->AddNewTrail(up, down); // 위치정보 넣어줘야함!

		//cout << down.x << " " << down.y << " " << down.z << endl;
		//cout << up.x << " " << up.y << " " << up.z << endl;

		trail->UpdateTrail();

		trail->RenderTrail();
		box->DrawSubset(0);


		g_pd3dDevice->EndScene();
	}

	// Present the backbuffer contents to the display
	g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}

#pragma region OTHERS
//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Releases all previously initialized objects
//-----------------------------------------------------------------------------
VOID Cleanup()
{
	/*if (trail->vb != NULL)
		trail->vb->Release();*/

	if (trail != nullptr)
		delete trail;

	if (g_pd3dDevice != NULL)
		g_pd3dDevice->Release();

	if (g_pD3D != NULL)
		g_pD3D->Release();
}


LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		Cleanup();
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

//INT WINAPI wWinMain( HINSTANCE hInst, HINSTANCE, LPWSTR, INT )
int main(void)
{
	//UNREFERENCED_PARAMETER( hInst );

	// Register the window class
	WNDCLASSEX wc =
	{
		sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
		GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
		L"Trail", NULL
	};
	RegisterClassEx(&wc);

	// Create the application's window
	HWND hWnd = CreateWindow(L"Trail", L"Trail",
		WS_OVERLAPPEDWINDOW, 100, 100, 600, 600,
		NULL, NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	if (SUCCEEDED(InitD3D(hWnd)))
	{
		// Create the scene geometry
		if (SUCCEEDED(InitGeometry()))
		{
			// Show the window
			ShowWindow(hWnd, SW_SHOWDEFAULT);
			UpdateWindow(hWnd);

			// Enter the message loop
			MSG msg;
			ZeroMemory(&msg, sizeof(msg));
			while (msg.message != WM_QUIT)
			{
				if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				else
					Render();
			}
		}
	}

	UnregisterClass(L"D3D Tutorial", wc.hInstance);
	return 0;
}

#pragma endregion