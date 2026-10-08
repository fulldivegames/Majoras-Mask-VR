#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "runtime.h"
#include "source_blend.h"
#include "native_blend.h"
#include "presentation.h"
#include "mask_effects.h"
#include "visibility.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"
#include <fstream>
#include <string>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void hr(HRESULT value){if(FAILED(value))throw std::runtime_error("UI offscreen rendering failed");}
#include "wearable_mask_preview_test.h"
int main(int argc,char** argv){
 if(argc!=2 && argc!=3)return 2;
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
 hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1024,768};io.DeltaTime=1.f/90;
 ImGui_ImplDX11_Init(device.Get(),context.Get());ImGui_ImplDX11_NewFrame();ImGui::NewFrame();
 D3D11_TEXTURE2D_DESC desc{};desc.Width=1024;desc.Height=768;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> target,readback;hr(device->CreateTexture2D(&desc,nullptr,&target));ComPtr<ID3D11RenderTargetView> view;hr(device->CreateRenderTargetView(target.Get(),nullptr,&view));
 desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;hr(device->CreateTexture2D(&desc,nullptr,&readback));
 if(argc==3 && std::string(argv[2])=="--wearable-only") {
  CheckWearableMaskPixels(argv[1],device.Get(),context.Get(),view.Get(),target.Get(),readback.Get());
  ImGui::EndFrame();ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();return 0;
 }
 if(argc==3 && (std::string(argv[2])=="--setup-only" || std::string(argv[2])=="--notes-first" || std::string(argv[2])=="--notes-last")) {
  mmvr::setupGuideVisible=std::string(argv[2])=="--setup-only";
  auto& menu=mmvr::GetMenu();menu.tab=mmvr::SystemTab;
  if(!mmvr::setupGuideVisible){menu.CollapseAll();menu.expanded[39]=true;
    for(int i=0;i<menu.VisibleRows();++i)if(menu.VisibleSetting(i)==mmvr::ReleaseNotesFirstRow+(std::string(argv[2])=="--notes-last"?7:0)){menu.row=i;break;}
    menu.Normalize();}
  const float clear[4]={0,0,0,0};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
  ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});
  mmvr::presentation::Draw(list,{mmvr::UiKind::Menu,1024,768},nullptr,{});
  list.PopClipRect();list.PopTextureID();ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
  context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
  std::ofstream output(std::string(argv[1])+"/setup-guide.rgba",std::ios::binary);
  for(unsigned y=0;y<768;++y)output.write(static_cast<const char*>(mapped.pData)+y*mapped.RowPitch,1024*4);
  context->Unmap(readback.Get(),0);ImGui::EndFrame();ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();return 0;
 }
 for(int leftMode=0;leftMode<2;++leftMode)for(int tab=0;tab<mmvr::TabCount;++tab)for(int page=0;page<(tab==mmvr::ControlsTab?7:3);++page){
  if(leftMode&&(tab!=0||page!=0))continue;
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,leftMode);
  auto& menu=mmvr::GetMenu();menu={};menu.tab=tab;
  if(page)for(int section=0;section<mmvr::MenuSectionCount;++section)menu.expanded[section]=true;
  menu.row=page==0?0:page==1?menu.VisibleRows()/2:menu.VisibleRows()-1;menu.Normalize();
  auto& binding=mmvr::GetBindingEditor();binding.Cancel();
  if(tab==mmvr::ControlsTab && page>=3){binding.Begin(0);binding.phase=mmvr::BindingEditor::Phase(page-2);binding.source=11;}
  const float clear[4]={0,0,0,0};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
  ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});
  mmvr::presentation::Draw(list,{mmvr::UiKind::Menu,1024,768},nullptr,{});
  list.PopClipRect();list.PopTextureID();ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
  context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
  std::ofstream output(std::string(argv[1])+(leftMode?"/menu-left-tab-":"/menu-tab-")+std::to_string(tab)+"-page-"+std::to_string(page+1)+".rgba",std::ios::binary);
  for(unsigned y=0;y<768;++y)output.write(static_cast<const char*>(mapped.pData)+y*mapped.RowPitch,1024*4);
  context->Unmap(readback.Get(),0);
 }
 mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,0);
 for(int worn:{1,0}){
  mmvr::SetMaskInventory(-1,worn?0x3c:-1,false);mmvr::GetSettings().Set(mmvr::Setting::HudOpacity,1);
  const float clear[4]={0,0,0,0};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
  ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});
  mmvr::presentation::Draw(list,{mmvr::UiKind::MaskStatus,1024,768},nullptr,{});list.PopClipRect();list.PopTextureID();
  ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
  context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
  std::ofstream output(std::string(argv[1])+"/mask-status-"+std::to_string(worn)+".rgba",std::ios::binary);
  for(unsigned y=0;y<768;++y)output.write(static_cast<const char*>(mapped.pData)+y*mapped.RowPitch,1024*4);context->Unmap(readback.Get(),0);
 }
 mmvr::GetBindingEditor().Cancel();
 // Draw translucent geometry onto a transparent native pause target using production alpha factors.
 {
  D3D11_BLEND_DESC blend{};auto& d=blend.RenderTarget[0];d.BlendEnable=TRUE;
  d.SrcBlend=D3D11_BLEND_SRC_ALPHA;d.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;d.BlendOp=D3D11_BLEND_OP_ADD;d.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
  mmvr::NativeLayerAlpha(d);ComPtr<ID3D11BlendState> state;hr(device->CreateBlendState(&blend,&state));
  mmvr::SourceBlendBinding binding{context.Get(),state.Get()};
  for(float background:{0.f,1.f}){
   const float clear[4]={0,0,0,background};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
   ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});
   list.AddCallback([](const ImDrawList*,const ImDrawCmd* cmd){mmvr::SourceBlendBinding::Apply(cmd->UserCallbackData);},&binding);
   list.AddRectFilled({200,200},{800,600},IM_COL32(200,100,50,128));
   list.AddCallback(ImDrawCallback_ResetRenderState,nullptr);list.PopClipRect();list.PopTextureID();
   ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
   context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
   auto* actual=static_cast<uint8_t*>(mapped.pData)+384*mapped.RowPitch+512*4;
   bool good=actual[0]>=99&&actual[0]<=101&&(background==0?(actual[3]>=127&&actual[3]<=129):actual[3]==255);
   context->Unmap(readback.Get(),0);if(!good)throw std::runtime_error("Native pause geometry lost alpha coverage");
  }
 }
 // Pixel-level regression: theater ignores native zero alpha; HUD scales premultiplied color once.
 {
  D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  uint8_t pixel[4]={180,80,40,0};D3D11_SUBRESOURCE_DATA initial{pixel,4,0};ComPtr<ID3D11Texture2D> source;ComPtr<ID3D11ShaderResourceView> srv;
  hr(device->CreateTexture2D(&td,&initial,&source));hr(device->CreateShaderResourceView(source.Get(),nullptr,&srv));mmvr::SourceBlendCache blends;
  for(bool theater:{true,false}){
   const float clear[4]={0,0,0,theater?1.f:0.f};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
   mmvr::GetSettings().Set(mmvr::Setting::HudOpacity,.5f);mmvr::SourceBlendBinding binding{context.Get(),blends.Get(context.Get(),theater)};
   mmvr::UiDrawFrame frame{theater?mmvr::UiKind::Theater:mmvr::UiKind::Hud,1024,768,reinterpret_cast<uintptr_t>(srv.Get()),mmvr::SourceBlendBinding::Apply,&binding};
   ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});mmvr::presentation::Draw(list,frame,nullptr,{});list.PopClipRect();list.PopTextureID();
   ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
   context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
   auto* actual=static_cast<uint8_t*>(mapped.pData)+384*mapped.RowPitch+512*4;
   bool good=theater?(actual[0]==180&&actual[1]==80&&actual[2]==40&&actual[3]==255):(actual[0]>=89&&actual[0]<=90&&actual[1]>=39&&actual[1]<=40&&actual[3]==0);
   context->Unmap(readback.Get(),0);if(!good)throw std::runtime_error("VR source alpha regression");
  }
 }
 {
  D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  uint8_t pixel[4]={180,80,40,0};D3D11_SUBRESOURCE_DATA initial{pixel,4,0};ComPtr<ID3D11Texture2D> source;ComPtr<ID3D11ShaderResourceView> srv;
  hr(device->CreateTexture2D(&td,&initial,&source));hr(device->CreateShaderResourceView(source.Get(),nullptr,&srv));mmvr::SourceBlendCache blends;
  for(float opacity:{0.f,.5f,1.f}){
   mmvr::GetSettings().Set(mmvr::Setting::HudOpacity,opacity);mmvr::SetLensVision(1);
   const float clear[4]={0,0,0,1};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
   mmvr::SourceBlendBinding binding{context.Get(),blends.Get(context.Get(),true)};
   mmvr::UiDrawFrame frame{mmvr::UiKind::Vision,1024,768,reinterpret_cast<uintptr_t>(srv.Get()),mmvr::SourceBlendBinding::Apply,&binding};
   ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});mmvr::presentation::Draw(list,frame,nullptr,{});list.PopClipRect();list.PopTextureID();
   ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
   context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
   bool good=true;for(int y:{0,384,767})for(int x:{0,512,1023}){auto* p=static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4;std::ofstream(std::string(argv[1])+"/lens-pixels.log",std::ios::app)<<opacity<<" "<<x<<","<<y<<" = "<<int(p[0])<<","<<int(p[1])<<","<<int(p[2])<<","<<int(p[3])<<"\n";if(x==512&&y==384)good&=p[0]==180&&p[1]==80&&p[2]==40&&p[3]==255;else good&=p[0]>=147&&p[0]<=151&&p[1]>=54&&p[1]<=57&&p[2]>=68&&p[2]<=71&&p[3]==255;}
   context->Unmap(readback.Get(),0);if(!good)throw std::runtime_error("Lens aperture must remain clear with a purple surround independent of HUD opacity");
  }
  mmvr::SetLensVision(0);
  for(float opacity:{0.f,1.f})for(float fade:{0.f,1.f}){
   mmvr::GetSettings().Set(mmvr::Setting::HudOpacity,opacity);mmvr::SetViewTool(2,1,fade);
   const float clear[4]={0,0,0,1};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
   mmvr::SourceBlendBinding binding{context.Get(),blends.Get(context.Get(),true)};
   mmvr::UiDrawFrame frame{mmvr::UiKind::Vision,1024,768,reinterpret_cast<uintptr_t>(srv.Get()),mmvr::SourceBlendBinding::Apply,&binding};
   ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});mmvr::presentation::Draw(list,frame,nullptr,{});list.PopClipRect();list.PopTextureID();
   ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
   context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
   bool good=true;for(int y:{0,384,767})for(int x:{0,512,1023}){auto* p=static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4;bool clearCenter=fade==0&&x==512&&y==384;good&=clearCenter?(p[0]==180&&p[1]==80&&p[2]==40&&p[3]==255):(p[0]==0&&p[1]==0&&p[2]==0&&p[3]==255);}
   context->Unmap(readback.Get(),0);if(!good)throw std::runtime_error("Telescope must black out every eye edge and fade the aperture beyond native angles");
  }
  mmvr::SetViewTool(0,1,0);
 }
 {
  D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  uint8_t red[4]={255,0,0,0},blue[4]={0,0,255,255};D3D11_SUBRESOURCE_DATA a{red,4,0},b{blue,4,0};ComPtr<ID3D11Texture2D> source,history;ComPtr<ID3D11ShaderResourceView> sourceView,historyView;
  hr(device->CreateTexture2D(&td,&a,&source));hr(device->CreateTexture2D(&td,&b,&history));hr(device->CreateShaderResourceView(source.Get(),nullptr,&sourceView));hr(device->CreateShaderResourceView(history.Get(),nullptr,&historyView));
  const float clear[4]={0,0,0,1};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
  mmvr::SourceBlendCache blends;mmvr::SourceBlendBinding binding{context.Get(),blends.Get(context.Get(),true)};
  mmvr::UiDrawFrame frame{mmvr::UiKind::MotionBlur,1024,768,reinterpret_cast<uintptr_t>(sourceView.Get()),mmvr::SourceBlendBinding::Apply,&binding,{},reinterpret_cast<uintptr_t>(historyView.Get()),.5f};
  ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});mmvr::presentation::Draw(list,frame,nullptr,{});list.PopClipRect();list.PopTextureID();
  ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
  context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
  bool good=true;for(int y:{1,384,766})for(int x:{1,512,1022}){auto* p=static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4;good&=p[0]>=127&&p[0]<=129&&p[1]==0&&p[2]>=126&&p[2]<=128&&p[3]==255;}
  context->Unmap(readback.Get(),0);if(!good)throw std::runtime_error("Eye motion history failed full-view alpha composition");
 }
 // Transformation filters cover the entire eye and are independent of HUD opacity.
 for(int style=1;style<=5;++style)for(float opacity:{0.f,1.f}){
  mmvr::GetSettings().Set(mmvr::Setting::HudOpacity,opacity);auto tint=mmvr::MaskEffectTint(style,.5f);mmvr::SetWorldTint(tint);
  const float clear[4]={0,0,0,0};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
  ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});
  // Null source contributes no world color in this isolated overlay test.
  mmvr::presentation::Draw(list,{mmvr::UiKind::Vision,1024,768},nullptr,{});list.PopClipRect();list.PopTextureID();
  ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
  context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
  bool good=true;for(int y:{2,384,765})for(int x:{2,512,1021}){auto* pixel=static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4;for(int c=0;c<3;++c)good&=std::abs(int(pixel[c])-int(int(tint[c]*255)*127/255))<=2;}
  context->Unmap(readback.Get(),0);if(!good)throw std::runtime_error("Transformation eye filter coverage or opacity regression");
 }
 mmvr::SetWorldTint({});
 // The production screen-fade draw must cover every pixel regardless of HUD
 // opacity. This also checks premultiplied alpha for the compositor layer.
 _putenv_s("MMVR_NATIVE_TEST","1");mmvr::SetNativeTestTracking(true);
 mmvr::SetScene(true,nullptr,nullptr);
 mmvr::SetUiCallbacks([](const mmvr::UiDrawFrame&){},nullptr,nullptr);
 for(int world:{0,1})for(float opacity:{0.f,1.f})for(int alpha:{0,25,128,255}){
  mmvr::GetSettings().Set(mmvr::Setting::HudOpacity,opacity);MMVR_ResetScreenFade();
  if(!(world?MMVR_RecordWorldScreenFade(255,255,255,alpha,3):MMVR_RecordScreenFade(255,255,255,alpha)))throw std::runtime_error("Screen fade registration failed");
  const float clear[4]={0,0,0,0};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
  ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});
  mmvr::presentation::Draw(list,{mmvr::UiKind::ScreenFade,1024,768},nullptr,{});list.PopClipRect();list.PopTextureID();
  ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
  context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
  bool good=true;for(int y=0;y<768;++y)for(int x=0;x<1024;++x){auto* pixel=static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4;for(int c=0;c<4;++c)good&=std::abs(int(pixel[c])-int(std::lround((world?(1-std::pow(1-alpha/255.f,2)):alpha/255.f)*255)))<=1;}
  context->Unmap(readback.Get(),0);if(!good)throw std::runtime_error("Screen fade has holes or incorrect opacity");
 }
 MMVR_ResetScreenFade();MMVR_RecordScreenFade(255,0,0,128);MMVR_RecordScreenFade(0,0,255,128);
 auto composed=mmvr::ScreenFade();if(std::abs(composed[3]-.752f)>.003f||std::abs(composed[0]-.3325f)>.003f||std::abs(composed[2]-.6675f)>.003f)throw std::runtime_error("Layered native fades are not source-over");
 // Streak phase follows XR display time, even when native simulation and
 // ImGui's frame clock have not advanced between the two rendered samples.
 mmvr::SetSpeedStreaks(.1f);std::vector<uint8_t> previous;unsigned changed=0;
 for(int sample=0;sample<2;++sample){
  mmvr::CameraFrame camera;camera.trackingTime=sample*.08;mmvr::SetNativeTestCamera(camera);
  const float clear[4]={0,0,0,0};context->ClearRenderTargetView(view.Get(),clear);auto* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
  ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1024,768});
  mmvr::presentation::Draw(list,{mmvr::UiKind::Vision,1024,768},nullptr,{});list.PopClipRect();list.PopTextureID();
  ImDrawData data;data.Valid=true;data.DisplaySize={1024,768};data.FramebufferScale={1,1};data.AddDrawList(&list);ImGui_ImplDX11_RenderDrawData(&data);
  context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
  std::vector<uint8_t> pixels(1024*768*4);unsigned visible=0;bool opacity=true;
  for(int y=0;y<768;++y){std::memcpy(pixels.data()+y*1024*4,static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch,1024*4);}
  for(size_t i=0;i<pixels.size();i+=4){if(pixels[i+3]>5)++visible;opacity&=pixels[i+3]<=26;if(!previous.empty()&&pixels[i+3]!=previous[i+3])++changed;}
  context->Unmap(readback.Get(),0);
  std::ofstream(std::string(argv[1])+"/goron-streaks-"+std::to_string(sample)+".rgba",std::ios::binary).write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
  if(visible<1500||!opacity)throw std::runtime_error("Goron streaks missing or exceed requested ten percent opacity");
  previous=std::move(pixels);
 }
 if(changed<1500)throw std::runtime_error("Goron streaks did not advance with headset display time");
 mmvr::SetSpeedStreaks(0);mmvr::SetNativeTestCamera({});
 std::ofstream(std::string(argv[1])+"/goron-streaks.json")<<"{\"visible\":true,\"tenPercentOpacity\":true,\"xrPhaseChangedPixels\":"<<changed<<"}";
 MMVR_ResetScreenFade();MMVR_SetTheaterFadeComposition(1);
 if(MMVR_RecordScreenFade(255,255,255,255) || MMVR_RecordWorldScreenFade(255,255,255,255,2) || mmvr::ScreenFade()[3]!=0)
  throw std::runtime_error("Theater fade escaped into the headset overlay");
 MMVR_ResetScreenFade();mmvr::SetScene(false,nullptr,nullptr);mmvr::SetNativeTestTracking(false);
 if(mmvr::ScreenFade()[3]!=0||MMVR_RecordScreenFade(255,255,255,255))throw std::runtime_error("Desktop fade was intercepted without VR");
 std::ofstream(std::string(argv[1])+"/screen-fade.json")<<"{\"allPixelsCovered\":true,\"hudIndependent\":true,\"sourceOver\":true,\"desktopPreserved\":true}";
 ImGui::EndFrame();ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();
}
