#pragma once
#include "wearable_mask.h"
#include <filesystem>
// Isolated test executable only. RGBA inputs are exact PNG decodes prepared by
// check-wearable-masks.py; the native probe separately tests the shipped decoder.
inline void CheckWearableMaskPixels(const char* directory, ID3D11Device* device,
    ID3D11DeviceContext* context, ID3D11RenderTargetView* target, ID3D11Texture2D* output,
    ID3D11Texture2D* readback) {
    auto makeTexture=[&](unsigned w,unsigned h,const std::vector<uint8_t>& bytes) {
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{bytes.data(),w*4,0};
        ComPtr<ID3D11Texture2D> texture;hr(device->CreateTexture2D(&d,&data,&texture));
        ComPtr<ID3D11ShaderResourceView> srv;hr(device->CreateShaderResourceView(texture.Get(),nullptr,&srv));return srv;
    };
    auto background=makeTexture(1,1,{180,80,40,255});
    mmvr::SourceBlendCache blends;
    mmvr::SourceBlendBinding binding{context,blends.Get(context,true)};
    XrFovf eyes[2]{{-.95f,.65f,.8f,-.7f},{-.65f,.95f,.8f,-.7f}};
    unsigned checked=0;
    for(const auto& asset:mmvr::WearableMaskAssets) {
        std::ifstream in(std::filesystem::path(directory)/(std::string(asset.file)+".rgba"),std::ios::binary);
        uint32_t size[2]{};in.read(reinterpret_cast<char*>(size),sizeof(size));
        if(!in||size[0]>2048||size[1]>2048||!size[0]||!size[1])throw std::runtime_error("Invalid overlay test input");
        std::vector<uint8_t> pixels(size_t(size[0])*size[1]*4);
        in.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
        if(!in)throw std::runtime_error("Truncated overlay test input");
        auto texture=makeTexture(size[0],size[1],pixels);
        for(int eye=0;eye<2;++eye)for(float hudOpacity:{0.f,1.f})for(bool visible:{false,true}) {
            mmvr::GetSettings().Set(mmvr::Setting::HudOpacity,hudOpacity);
            const float clear[]{0,0,0,1};context->ClearRenderTargetView(target,clear);context->OMSetRenderTargets(1,&target,nullptr);
            mmvr::UiDrawFrame frame{mmvr::UiKind::WearableMask,1024,768,reinterpret_cast<uintptr_t>(background.Get()),
                mmvr::SourceBlendBinding::Apply,&binding,eyes[eye]};
            frame.wearableMaskTexture=visible?reinterpret_cast<uintptr_t>(texture.Get()):0;
            frame.wearableMaskUv=mmvr::WearableMaskUv(eyes[eye],eyes[0],eyes[1],asset.item);
            ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();
            list.PushTextureID(ImGui::GetIO().Fonts->TexID);list.PushClipRect({0,0},{1024,768});
            mmvr::presentation::Draw(list,frame,nullptr,{});list.PopClipRect();list.PopTextureID();
            ImDrawData draw;draw.Valid=true;draw.DisplaySize={1024,768};draw.FramebufferScale={1,1};draw.AddDrawList(&list);
            ImGui_ImplDX11_RenderDrawData(&draw);context->CopyResource(readback,output);
            D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback,0,D3D11_MAP_READ,0,&mapped));
            bool good=true;
            for(int y:{4,192,384,576,763})for(int x:{4,256,512,768,1019}) {
                const auto& uv=frame.wearableMaskUv;
                const float sx=(uv[0]+(x+.5f)/1024*(uv[2]-uv[0]))*size[0]-.5f;
                const float sy=(uv[1]+(y+.5f)/768*(uv[3]-uv[1]))*size[1]-.5f;
                const int ix=int(std::floor(sx)),iy=int(std::floor(sy));
                float rgba[4]{};
                for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx) {
                    float weight=(dx?sx-ix:1-(sx-ix))*(dy?sy-iy:1-(sy-iy));
                    size_t at=(size_t(std::clamp(iy+dy,0,int(size[1])-1))*size[0]+std::clamp(ix+dx,0,int(size[0])-1))*4;
                    for(int c=0;c<4;++c)rgba[c]+=pixels[at+c]*weight;
                }
                const auto* actual=static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch+x*4;
                const float alpha=visible?rgba[3]/255:0;
                const int base[]{180,80,40};
                for(int c=0;c<3;++c)good &= std::abs(actual[c]-(rgba[c]*alpha+base[c]*(1-alpha)))<=3;
                good &= actual[3]==255;
            }
            if(visible && hudOpacity==1) {
                std::ofstream image(std::filesystem::path(directory)/(std::string(asset.file)+"-eye"+std::to_string(eye)+".rgba"),std::ios::binary);
                for(int y=0;y<768;++y)image.write(static_cast<const char*>(mapped.pData)+size_t(y)*mapped.RowPitch,1024*4);
            }
            context->Unmap(readback,0);
            if(!good)throw std::runtime_error(std::string("Wearable overlay blend regression: ")+asset.file);
            ++checked;
        }
    }
    std::ofstream(std::filesystem::path(directory)/"pixels.json")<<"{\"passed\":true,\"masks\":19,\"compositions\":"<<checked
        <<",\"bothEyes\":true,\"hudOpacityIndependent\":true,\"clearOnRemoval\":true}";
}
