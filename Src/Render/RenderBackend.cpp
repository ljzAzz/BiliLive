#include "stdafx.h"
#include "RenderBackend.h"
#include "Dx12RenderBackend.h"

RenderBackend* RenderBackend::CreateRenderBackend(RenderBackendApi api, void* initData)
{
	switch (api) {
		case RenderBackendApi::Dx12:
			DX12::CreateDevice();
			return new Dx12RenderBackend(initData);
		default:
			return nullptr;
	}
}

