#include "Dx12Wrapper.h"
#include"Application.h"
#include<DirectXMath.h>
#include<d3dcompiler.h>
#include<cassert>
#include<memory>
#include"PMDActor.h"
#include"PMDRenderer.h"
#include<d3dx12.h>
#include"Helper.h"


#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"DirectXTex.lib")


using namespace DirectX;
using namespace std;



Dx12Wrapper::Dx12Wrapper(HWND hwnd): _hwnd(hwnd),
	_eye(0,15,-25),
	_target(0,10,0),
	_up(0,1,0)
{
	
}


Dx12Wrapper::~Dx12Wrapper()
{
}


bool 
Dx12Wrapper::Init() {
	ID3D12Debug* debug;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
	debug->EnableDebugLayer();
	debug->Release();

	HRESULT result=S_OK;
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	for (auto level : levels) {
		if (SUCCEEDED(
			D3D12CreateDevice(
				nullptr,
				level,
				IID_PPV_ARGS(&_dev)
			))) {
			break;
		}
	}
	if (_dev == nullptr) {
		return false;
	}

	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf())))) {
		return false;
	}

	//�R�}���h�L���[�̍쐬
	D3D12_COMMAND_QUEUE_DESC cmdQDesc = {};
	cmdQDesc.NodeMask = 0;//�A�_�v�^�[�}�X�N�Ȃ�
	cmdQDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cmdQDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	if (FAILED(_dev->CreateCommandQueue(&cmdQDesc, IID_PPV_ARGS(_cmdQue.ReleaseAndGetAddressOf())))) {
		return false;
	}

	Application& app = Application::Instance();
	auto wsize = app.GetWindowSize();
	DXGI_SWAP_CHAIN_DESC1 swDesc = {};
	swDesc.BufferCount = 2;//�t���b�v�Ɏg�p���鎆�͂Q��
	swDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swDesc.Flags = 0;
	swDesc.Width = wsize.width;
	swDesc.Height = wsize.height;
	swDesc.SampleDesc.Count = 1;
	swDesc.SampleDesc.Quality = 0;
	swDesc.Scaling = DXGI_SCALING_STRETCH;
	swDesc.Stereo = false;
	swDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQue.Get(),
		_hwnd,
		&swDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)(_swapchain.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		return false;
	}

	if (!CreateRenderTargetView()) {
		return false;
	}

	if (!CreateDepthBuffer()) {
		return false;
	}
	if (!CreateDSV()) {
		return false;
	}

	//�R�}���h���X�g�̍쐬
	if (!CreateCommandList()) {
		return false;
	}

	//�t�F���X�̍쐬
	_fenceValue = 0;//��r�p
	result = _dev->CreateFence(
		_fenceValue, //�����l
		D3D12_FENCE_FLAG_NONE, //���Ƀt���O�Ȃ�
		IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()));//_fence�����
	assert(SUCCEEDED(result));


	if (!CreateTransformConstantBuffer()) {
		return false;
	}

	if (!CreateWhiteTexture()) {
		return false;
	}
	if (!CreateBlackTexture()) {
		return false;
	}
	if (!CreateGradationTexture()) {
		return false;
	}
	if (!CreateTransformBufferView()) {
		return false;
	}

	//�{�P�p�����[�^(�萔�o�b�t�@���\�[�X)
	if (!CreateBokehParamResource()) {
		return false;
	}

	//�y���|���p
	if (!CreatePeraResourcesAndView()) {
		return false;
	}
	if (!CreatePeraVertex()) {
		return false;
	}
	if (!CreatePeraPipeline()) {
		return false;
	}



	return true;

}
bool 
Dx12Wrapper::CreatePeraVertex() {
	struct PeraVertex {
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};
	PeraVertex pv[4] = { {{-1,-1,0.1},{0,1}},
						{{-1,1,0.1},{0,0}},
						{{1,-1,0.1},{1,1}},
						{{1,1,0.1},{1,0}} };
	auto result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(pv)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_peraVB.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	//�}�b�v�Y��Ă܂����B
	PeraVertex* mappedPera = nullptr;
	_peraVB->Map(0, nullptr, (void**)&mappedPera);
	copy(begin(pv), end(pv), mappedPera);
	_peraVB->Unmap(0, nullptr);

	_peraVBV.BufferLocation = _peraVB->GetGPUVirtualAddress();
	_peraVBV.SizeInBytes = sizeof(pv);
	_peraVBV.StrideInBytes = sizeof(PeraVertex);
	return true;
}

ComPtr<ID3D12Resource> 
Dx12Wrapper::WhiteTexture() {
	return _whiteTex;
}
ComPtr<ID3D12Resource> 
Dx12Wrapper::BlackTexture() {
	return _blackTex;
}
ComPtr<ID3D12Resource> 
Dx12Wrapper::GradTexture() {
	return _gradTex;
}

bool 
Dx12Wrapper::CreateRenderTargetView() {
	DXGI_SWAP_CHAIN_DESC1 swDesc = {};
	auto result = _swapchain->GetDesc1(&swDesc);

	//�����_�[�^�[�Q�b�g�r���[�p�̃q�[�v���쐬
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.NodeMask = 0;
	desc.NumDescriptors = swDesc.BufferCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = _dev->CreateDescriptorHeap(&desc,
						IID_PPV_ARGS(&_rtvDescHeap));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	_backBuffers.resize(swDesc.BufferCount);

	auto handle=_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize=_dev->GetDescriptorHandleIncrementSize(
							D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//�X���b�v�`�F�C������A�t���b�v�Ώۂ̃��\�[�X���擾
	for (int i = 0; i < swDesc.BufferCount; ++i) {
		//GetBuffer�̓X���b�v�`�F�C���������Ă�i�Ԗڂ�
		//���\�[�X��������ɓ���Ă����
		result = _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backBuffers[i]));
		assert(SUCCEEDED(result));
		if (FAILED(result)) {
			return false;
		}
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		_dev->CreateRenderTargetView(_backBuffers[i], &rtvDesc, handle);
		handle.ptr += incSize;
	}

	return true;

}

bool 
Dx12Wrapper::CreateCommandList() {
	auto result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
										IID_PPV_ARGS(_cmdAlloc.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		_cmdAlloc.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	return true;
}

void 
Dx12Wrapper::PostDrawToPera1() {
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_peraResource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}

bool 
Dx12Wrapper::PreDrawToPera1() {
	_cmdList->ResourceBarrier(1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(_peraResource.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET));
	auto rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvHeapPointer = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvHeapPointer);
	//�N���A�J���[		 R   G   B   A
	float clsClr[4] = { 0.5,0.5,0.5,1.0 };
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clsClr, 0, nullptr);
	_cmdList->ClearDepthStencilView(dsvHeapPointer,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	return true;
}

bool
Dx12Wrapper::Clear() {
	//�o�b�N�o�b�t�@�̃C���f�b�N�X���擾����
	auto bbIdx=_swapchain->GetCurrentBackBufferIndex();

	Barrier(_backBuffers[bbIdx],
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	

	auto rtvHeapPointer=_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr+=bbIdx*_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);
	
	//�N���A�J���[		 R   G   B   A
	float clsClr[4] = { 0.2,0.5,0.5,1.0 };
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clsClr, 0, nullptr);
	//_cmdList->ClearDepthStencilView(_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
	//	D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	return true;
}

void Dx12Wrapper::Barrier(ID3D12Resource* p,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after){
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(p, before, after, 0));
}

void 
Dx12Wrapper::Flip() {
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

	Barrier(_backBuffers[bbIdx],
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	_cmdList->Close();
	ID3D12CommandList* cmds[] = { _cmdList.Get() };
	_cmdQue->ExecuteCommandLists(1, cmds);
	
	WaitForCommandQueue();

	_cmdAlloc->Reset();
	_cmdList->Reset(_cmdAlloc.Get(), nullptr);

	//Present�֐����ADxLib�ɂ�����ScreenFlip�݂����Ȃ���ł��B
	//Present�̑��������u���񐂒�������҂��v�ł��B
	//���������M�����Ă̂̓u���E���ǂ̎���ɑ��������������
	//����1/60�������񂾂��ǁA���͂���Ȃ̂Ȃ��̂ő҂��Ȃ�
	auto result = _swapchain->Present(0, 0);
	assert(SUCCEEDED(result));
}

void Dx12Wrapper::WaitForCommandQueue()
{
	//���ɏ���Ƃ���_fenceValue��0�������Ƃ��܂�
	_cmdQue->Signal(_fence.Get(), ++_fenceValue);
	//���̖��ߒ���ł�_fenceValue��1�ŁA
	//GetCompletedValue�͂܂�0�ł��B
	if (_fence->GetCompletedValue() < _fenceValue) {
		//�����܂��I����ĂȂ��Ȃ�A�C�x���g�҂����s��
		//�����̂��߂̃C�x���g�H���Ƃ��̂��߂�_fenceValue
		auto event = CreateEvent(nullptr, false, false, nullptr);
		//�t�F���X�ɑ΂��āACompletedValue��_fenceValue��
		//�Ȃ�����w��̃C�x���g�𔭐�������Ƃ������߁�
		_fence->SetEventOnCompletion(_fenceValue, event);
		//���܂��C�x���g�������Ȃ�
		//���C�x���g����������܂ő҂�
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
}

void 
Dx12Wrapper::SetFov(float angle) {
	_fov = angle;
}
void 
Dx12Wrapper::SetEyePosition(float x, float y, float z) {
	_eye.x = x;
	_eye.y = y;
	_eye.z = z;
}
void 
Dx12Wrapper::MoveEyePosition(float x, float y, float z) {
	_eye.x += x;
	_eye.y += y;
	_eye.z += z;
	_target.x += x;
	_target.y += y;
	_target.z += z;

}

bool 
Dx12Wrapper::CreateDepthBuffer() {
	D3D12_HEAP_PROPERTIES heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	auto wsize = Application::Instance().GetWindowSize();
	D3D12_RESOURCE_DESC resdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, wsize.width, wsize.height);
	resdesc.Flags= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE cv = {};
	cv.DepthStencil.Depth = 1.0f;
	cv.Format = DXGI_FORMAT_D32_FLOAT;

	auto result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&cv,
		IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	else {
		return true;
	}
}
bool 
Dx12Wrapper::CreateDSV() {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	auto result=_dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
	viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	viewDesc.Flags = D3D12_DSV_FLAG_NONE;
	viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	_dev->CreateDepthStencilView(_depthBuffer.Get(),
		&viewDesc,
		_dsvHeap->GetCPUDescriptorHandleForHeapStart());


	return true;
}

//�`��
void
Dx12Wrapper::DrawToPera1(shared_ptr<PMDRenderer> renderer) {
	auto wsize = Application::Instance().GetWindowSize();

	SetCameraSetting();

	ID3D12DescriptorHeap* heaps[] = { _sceneHeap.Get() };

	heaps[0] = _sceneHeap.Get();
	_cmdList->SetDescriptorHeaps(1, heaps);
	auto sceneHandle = _sceneHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(1, sceneHandle);

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.width, wsize.height);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, wsize.width, wsize.height);
	_cmdList->RSSetScissorRects(1, &rc);//�V�U�[(�؂蔲��)��`
}

bool 
Dx12Wrapper::CreatePeraPipeline() {
	D3D12_DESCRIPTOR_RANGE range[2] = {};
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//t
	range[0].BaseShaderRegister = 0;//0
	range[0].NumDescriptors = 1;

	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//b
	range[1].BaseShaderRegister = 0;//0
	range[1].NumDescriptors = 1;

	D3D12_ROOT_PARAMETER rp[2] = {};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rp[0].DescriptorTable.pDescriptorRanges = &range[0];
	rp[0].DescriptorTable.NumDescriptorRanges = 1;

	rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//
	rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rp[1].DescriptorTable.pDescriptorRanges = &range[1];
	rp[1].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 2;
	rsDesc.pParameters = rp;
	
	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0);
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	

	ComPtr<ID3DBlob> rsBlob;
	ComPtr<ID3DBlob> errBlob;
	auto result=D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, rsBlob.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errBlob.Get())) {
		assert(0);
		return false;
	}
	result = _dev->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(_peraRS.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	ComPtr<ID3DBlob> vs;
	ComPtr<ID3DBlob> ps;
	result = D3DCompileFromFile(L"pera.hlsl", nullptr, nullptr, "VS", "vs_5_0", 0, 0, vs.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result,errBlob.Get())) {
		assert(0);
		return false;
	}
	result = D3DCompileFromFile(L"pera.hlsl", nullptr, nullptr, "PS", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errBlob.Get())) {
		assert(0);
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	gpsDesc.DepthStencilState.DepthEnable = false;
	gpsDesc.DepthStencilState.StencilEnable = false;
	D3D12_INPUT_ELEMENT_DESC layout[2] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};
	gpsDesc.InputLayout.NumElements = _countof(layout);
	gpsDesc.InputLayout.pInputElementDescs = layout;
	gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpsDesc.SampleDesc.Count = 1;
	gpsDesc.SampleDesc.Quality = 0;
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	gpsDesc.pRootSignature = _peraRS.Get();
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_peraPipeline.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}
	result = D3DCompileFromFile(L"pera.hlsl", nullptr, nullptr, "VerticalBokehPS", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errBlob.Get())) {
		assert(0);
		return false;
	}
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_peraPipeline2.ReleaseAndGetAddressOf()));

	 

	return true;
}

void 
Dx12Wrapper::DrawHorizontalBokeh() {//���{�P�摜��`�悷��
	//�o�b�N�o�b�t�@�̃C���f�b�N�X���擾����
	Barrier(_peraResource2.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	
	auto rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);

	//�N���A�J���[		 R   G   B   A
	float clsClr[4] = { 0.5,0.5,0.5,1.0 };
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clsClr, 0, nullptr);

	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.width, wsize.height);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, wsize.width, wsize.height);
	_cmdList->RSSetScissorRects(1, &rc);//�V�U�[(�؂蔲��)��`

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetDescriptorHeaps(1, _peraRegisterHeap.GetAddressOf());

	auto handle = _peraRegisterHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(0, handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);

	_cmdList->SetPipelineState(_peraPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);

	Barrier(_peraResource2.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

}

//�`��
void 
Dx12Wrapper::Draw(shared_ptr<PMDRenderer> renderer) {
	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp=CD3DX12_VIEWPORT(0.0f,0.0f,wsize.width,wsize.height);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, wsize.width, wsize.height);
	_cmdList->RSSetScissorRects(1,&rc);//�V�U�[(�؂蔲��)��`
	
	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetDescriptorHeaps(1, _peraRegisterHeap.GetAddressOf());
	auto handle = _peraRegisterHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(0, handle);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);
	_cmdList->SetPipelineState(_peraPipeline2.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);
}

void Dx12Wrapper::SetCameraSetting()
{
	auto wsize = Application::Instance().GetWindowSize();
	_mappedScene->eye = _eye;
	_mappedScene->view = XMMatrixLookAtLH(
		XMLoadFloat3(&_eye),
		XMLoadFloat3(&_target),
		XMLoadFloat3(&_up));
	_mappedScene->proj = _mappedScene->proj = XMMatrixPerspectiveFovLH(
		_fov,
		static_cast<float>(wsize.width) / static_cast<float>(wsize.height),
		1.0f,
		1000.0f);
}

bool
Dx12Wrapper::CreateTextureFromImageData(const DirectX::Image* img,ComPtr<ID3D12Resource>& buff,bool isDiscrete) {
	//�܂�WriteToSubresource�����ōl����B
	D3D12_HEAP_PROPERTIES heapprop = {};

	if (isDiscrete) {
		//�O���{���f�B�X�N���[�g�̏ꍇ��DEFAULT�ō���Ă�����
		//�ォ����UPLOAD�o�b�t�@�𒆊ԃo�b�t�@(UPLOAD)�Ƃ��ėp��
		//CopyTextureRegion�œ]������
		heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	}
	else {//��̌^(UMA)
		heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;//�e�N�X�`���p
		heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapprop.CreationNodeMask = 0;
		heapprop.VisibleNodeMask = 0;
	}

	//�ŏI�������ݐ惊�\�[�X�̐ݒ�
	D3D12_RESOURCE_DESC resdesc =
	CD3DX12_RESOURCE_DESC::Tex2D(img->format,img->width,img->height);

	auto result = S_OK;
	if (isDiscrete) {
		result = _dev->CreateCommittedResource(&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	}else{
		result = _dev->CreateCommittedResource(&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	}

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	return true;
}

bool 
Dx12Wrapper::CreateWhiteTexture() {
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;//�e�N�X�`���p
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;
	
	D3D12_RESOURCE_DESC resdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4);

	auto result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(_whiteTex.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	////��
	vector<uint8_t> col(4*4*4);
	fill(col.begin(), col.end(), 0xff);
	result = _whiteTex->WriteToSubresource(0, nullptr, col.data(),4*4,col.size());
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	return true;

}

bool
Dx12Wrapper::CreateBlackTexture() {
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;//�e�N�X�`���p
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4);
	
	auto result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(_blackTex.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	//��
	vector<uint8_t> col(4 * 4 * 4);
	fill(col.begin(), col.end(), 0);
	result = _blackTex->WriteToSubresource(0, nullptr, col.data(), 4 * 4, col.size());
	if (!CheckResult(result)) {
		return false;
	}
	return true;
}

bool
Dx12Wrapper::CreateGradationTexture() {
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;//�e�N�X�`���p
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 256);

	auto result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(_gradTex.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	struct Color {
		uint8_t r, g, b, a;
		Color(uint8_t inr,
			uint8_t ing, 
			uint8_t inb, 
			uint8_t ina ):r(inr),
			g(ing) ,
			b(inb) ,
			a(ina) {}
		Color() {}
	};
	//�������O���f
	vector<Color> col(4 * 256);
	auto it = col.begin();
	for (int i = 255; i >=0; --i) {
		fill(it,it+4, Color(i,i,i,255));
		it += 4;
	}
	result = _gradTex->WriteToSubresource(0, nullptr, col.data(), sizeof(Color) * 4, sizeof(Color)*col.size());
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	return true;
}

XMVECTOR 
Dx12Wrapper::GetCameraPosition() {
	return XMLoadFloat3(&_eye);
}

bool
Dx12Wrapper::CreateBokehParamResource() {
	auto weights=GetGaussianWeights(8, 5.0f);
	auto result=_dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(AligmentedValue(sizeof(weights[0])*weights.size(),D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_bokehParamResource.ReleaseAndGetAddressOf())
	);
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}
	float* mappedWeight = nullptr;
	result = _bokehParamResource->Map(0,nullptr,(void**)&mappedWeight);
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	copy(weights.begin(), weights.end(), mappedWeight);
	_bokehParamResource->Unmap(0, nullptr);
	return true;
}

bool
Dx12Wrapper::LoadPictureFromFile(wstring filepath, ComPtr<ID3D12Resource>& buff) {

	//�����p�X�w�肪������}�b�v�ɂ��łɑ��݂���
	//�f�[�^����󂯎��(�t���C�E�F�C�g�p�^�[��)
	auto it = _textureTable.find(filepath);
	if (it != _textureTable.end()) {
		buff = _textureTable[filepath];
		return true;
	}


	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	HRESULT result = S_OK;
	//DXT���k�̍ۂɂ͓���ȏ������K�v
	//(4x4�s�N�Z���P�u���b�N�ƂȂ邽�ߍ���1/4�ɂȂ�)
	bool isDXT=false;
	auto ext = GetExtension(filepath);
	if (ext == L"tga") {
		result = LoadFromTGAFile(filepath.c_str(),
			&metadata,
			scratchImg);
	}
	else if (ext == L"dds") {
		result = LoadFromDDSFile(filepath.c_str(),0,
			&metadata,
			scratchImg);
		isDXT = true;
	}
	else {
		result = LoadFromWICFile(filepath.c_str(),
			0,
			&metadata,
			scratchImg);
	}
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	auto img = scratchImg.GetImage(0,0,0);
	
	bool isDescrete = true;

	if (!CreateTextureFromImageData(img,buff, isDescrete)) {
		return false;
	}

	if (!isDescrete) {
		result = buff->WriteToSubresource(0, 
			nullptr, 
			img->pixels, 
			img->rowPitch, 
			img->slicePitch);
	}
	else {
		
		//�]���̂��߂̒��ԃo�b�t�@���쐬����
		D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(
			AligmentedValue(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*img->height);
		Microsoft::WRL::ComPtr<ID3D12Resource> internalBuffer = nullptr;
		result = _dev->CreateCommittedResource(&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(internalBuffer.ReleaseAndGetAddressOf()));
		assert(SUCCEEDED(result));
		if (FAILED(result)) {
			return false;
		}
		uint8_t* mappedInternal = nullptr;
		internalBuffer->Map(0, nullptr, (void**)&mappedInternal);
		auto address = img->pixels;
		uint32_t height = isDXT ? img->height/4 : img->height;
		for (int i = 0; i < height; ++i) {
			copy_n(address, img->rowPitch, mappedInternal);
			address += img->rowPitch;
			mappedInternal += AligmentedValue(img->rowPitch,
				D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		}
		internalBuffer->Unmap(0, nullptr);

		D3D12_TEXTURE_COPY_LOCATION src = {}, dst = {};
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.pResource = internalBuffer.Get();
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Width = img->width;
		src.PlacedFootprint.Footprint.Height = img->height;
		src.PlacedFootprint.Footprint.RowPitch =
			AligmentedValue(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		src.PlacedFootprint.Footprint.Depth = metadata.depth;
		src.PlacedFootprint.Footprint.Format = img->format;

		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;
		dst.pResource = buff.Get();

		_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

		Barrier(buff.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		_cmdList->Close();
		ID3D12CommandList* cmds[] = { _cmdList.Get() };
		_cmdQue->ExecuteCommandLists(1, cmds);
		WaitForCommandQueue();
		_cmdAlloc->Reset();
		_cmdList->Reset(_cmdAlloc.Get(), nullptr);
	}
	
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	_textureTable[filepath] = buff;

	return SUCCEEDED(result);
}
bool 
Dx12Wrapper::CreateTransformConstantBuffer() {
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(
		AligmentedValue(sizeof(SceneMatrix), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	);

	auto result = _dev->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_sceneCB.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	return true;
}

bool 
Dx12Wrapper::CreateTransformBufferView()
{
	//�萔�o�b�t�@�r���[�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	auto result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_sceneHeap.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}

	auto handle = _sceneHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
	viewDesc.BufferLocation = _sceneCB->GetGPUVirtualAddress();
	viewDesc.SizeInBytes = _sceneCB->GetDesc().Width;
	_dev->CreateConstantBufferView(&viewDesc, handle);

	_sceneCB->Map(0, nullptr, (void**)&_mappedScene);
	SetCameraSetting();
	
	return true;
}

bool 
Dx12Wrapper::CreatePeraResourcesAndView() {
	auto& bbuff=_backBuffers[0];
	auto resDesc=bbuff->GetDesc();//���Ƃ��Ǝg���Ă���o�b�N�o�b�t�@�̏��𗘗p����
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	float clsClr[4] = { 0.5,0.5,0.5,1.0 };//�����_�����O���̃N���A�l�Ɠ����l
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clsClr);
	auto result = _dev->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_peraResource.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	result = _dev->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_peraResource2.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		assert(0);
		return false;
	}


	//�����_�[�^�[�Q�b�g�r���[(RTV)�����
	//���������̑O�ɂŃX�N���v�^�q�[�v���K�v
	//���Ƃ��ƍ���Ă���q�[�v�̏��ł����ꖇ���
	auto heapDesc = _rtvDescHeap->GetDesc();
	heapDesc.NumDescriptors = 2;//�ł��f�X�N���v�^�͂P��
	result = _dev->CreateDescriptorHeap(&heapDesc, 
		IID_PPV_ARGS(_peraRTVHeap.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}
	
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	auto handle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	//�܂��f�X�N���v�^�q�[�v�݂̂ŁA�r���[������Ă��Ȃ��̂ō��
	_dev->CreateRenderTargetView(
		_peraResource.Get(),
		&rtvDesc, 
		handle);
	//2����
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_dev->CreateRenderTargetView(
		_peraResource2.Get(),
		&rtvDesc,
		handle);

	//�V�F�[�_���\�[�X�r���[�r���[�����
	//���������̑O�ɂŃX�N���v�^�q�[�v���K�v
	heapDesc.NumDescriptors = 3;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	result = _dev->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(_peraRegisterHeap.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	handle = _peraRegisterHeap->GetCPUDescriptorHandleForHeapStart();

	//�{�P�萔�o�b�t�@�r���[�ݒ�
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _bokehParamResource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = _bokehParamResource->GetDesc().Width;
	_dev->CreateConstantBufferView(&cbvDesc, handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = rtvDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	_dev->CreateShaderResourceView(_peraResource.Get(),
		&srvDesc,
		handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_dev->CreateShaderResourceView(_peraResource2.Get(),
		&srvDesc,
		handle);

	
	return true;

}