// Derived from Dear ImGui's Renderer Backend for DirectX9

#include "imgui_impl_source.h"
#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "keyvalues.h"
#include <imgui/imgui.h>

static IMaterial* g_pFontMat = nullptr;

void ImGui_ImplSource_SetupRenderState(IMatRenderContext* ctx, ImDrawData* draw_data)
{
    // Apply imgui's display dimensions 
    ctx->Viewport(draw_data->DisplayPos.x, draw_data->DisplayPos.y, draw_data->DisplaySize.x, draw_data->DisplaySize.y);

    // Setup orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    ctx->MatrixMode(MATERIAL_PROJECTION);
    ctx->PushMatrix();
    ctx->LoadIdentity();

    float L = draw_data->DisplayPos.x + 0.5f;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
    float T = draw_data->DisplayPos.y + 0.5f;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;
    ctx->Ortho(L, B, R, T, 0, 1);
    g_pMaterialSystem->GetRenderContext()->MatrixMode(MATERIAL_VIEW);
    g_pMaterialSystem->GetRenderContext()->PushMatrix();
    g_pMaterialSystem->GetRenderContext()->LoadIdentity();
}

void ImGui_ImplSource_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    IMatRenderContext* ctx = materials->GetRenderContext();
    
    ImGui_ImplSource_SetupRenderState(ctx, draw_data);
    
    // We pass g_pFontMat, as we need something bound to be able to use dynamic meshes
    // This must be set as unbuffered, otherwise our scissor rect breaks
    IMesh* mesh = ctx->GetDynamicMesh(false, nullptr, nullptr, g_pFontMat);
    
    CMeshBuilder mb;

    // Render command lists
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n <draw_data->CmdListsCount; n++)
    {

        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        // Fill in the mesh
        mb.Begin(mesh, MATERIAL_TRIANGLES, cmd_list->VtxBuffer.Size, cmd_list->IdxBuffer.Size);

        const ImDrawVert* vtx_src = cmd_list->VtxBuffer.Data;
        for (int i = 0; i < cmd_list->VtxBuffer.Size; i++)
        {
            mb.Position3f(vtx_src->pos.x, vtx_src->pos.y, 0);
            mb.Color4ubv(reinterpret_cast<const unsigned char*>(&vtx_src->col));
            mb.TexCoord2f(0, vtx_src->uv.x, vtx_src->uv.y);
            mb.AdvanceVertex();
            vtx_src++;
        }

        // CMeshBuilder doesn't support FastIndexList, but CIndexBuilder does!
        ((CIndexBuilder&)mb).FastIndexList(cmd_list->IdxBuffer.Data, 0, cmd_list->IdxBuffer.Size);

        mb.End();
        
        // Draw the mesh
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplSource_SetupRenderState(ctx, draw_data);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                if (pcmd->GetTexID())
                {
                    Vector2D clipmin = { pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y };
                    Vector2D clipmax = { pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y };

                    // Avoid rendering completely clipped draws
                    if (clipmax.x <= clipmin.x || clipmax.y <= clipmin.y)
                        continue;
                    
                    ctx->Bind(pcmd->GetTexID());
                    ctx->SetScissorRect(clipmin.x, clipmin.y, clipmax.x, clipmax.y, true);
                    mesh->Draw(pcmd->IdxOffset, pcmd->ElemCount);
                }
            }
        }
       
    }

    // Disable our scissor now that we're done
    ctx->SetScissorRect(-1, -1, -1, -1, false);
}

bool ImGui_ImplSource_Init()
{
    // Setup backend capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_source";
    io.BackendFlags = ImGuiBackendFlags_None;

    ImGui_ImplSource_CreateDeviceObjects();
    return true;
}

void ImGui_ImplSource_Shutdown()
{
    ImGui_ImplSource_InvalidateDeviceObjects();
}

static bool ImGui_ImplSource_CreateFontsTexture()
{
    if (g_pFontMat)
        return true;

    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height, bytes_per_pixel;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);

    // Create a material for the texture
    materials->CreateNamedTextureFromBitsEx("imgui_font", TEXTURE_GROUP_VGUI, width, height, 1, IMAGE_FORMAT_RGBA8888, width * height * bytes_per_pixel, pixels, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_IGNORE_PICMIP | TEXTUREFLAGS_POINTSAMPLE);
    KeyValues* vmt = new KeyValues("UnlitGeneric");
    vmt->SetString("$basetexture", "imgui_font");
    vmt->SetInt("$nocull", 1);
    vmt->SetInt("$vertexcolor", 1);
    vmt->SetInt("$alphatest", 1);
    g_pFontMat = materials->CreateMaterial("imgui_font_mat", vmt);
    g_pFontMat->AddRef();

    // Store our identifier
    io.Fonts->SetTexID(g_pFontMat);

    return true;
}

bool ImGui_ImplSource_CreateDeviceObjects()
{
    if (!ImGui_ImplSource_CreateFontsTexture())
        return false;
    return true;
}

void ImGui_ImplSource_InvalidateDeviceObjects()
{
    if (g_pFontMat)
    {
        g_pFontMat->DecrementReferenceCount();
        g_pFontMat = 0;
    }
}
