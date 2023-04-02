#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    ImGui::ColorEdit3("Meshes base color", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat3("Light source position", m_uniforms.lightPos.M, -10.f, 10.f);

    ImGui::Text("\nAnti-aliasing");
    uint32_t prevMode = m_mode;
    ImGui::RadioButton("No AA",  reinterpret_cast<int*>(&m_mode), 0); ImGui::SameLine();
    ImGui::RadioButton("SSAAx2", reinterpret_cast<int*>(&m_mode), 1);
    if (prevMode != m_mode) {
      m_modeChanged = true;
      switch (m_mode) 
      {
      case 0:
        m_ssaaScale = 1;
        break;

      case 1:
        m_ssaaScale = 2;
        break;

      default:
        exit(1);
        break;
      }
    }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
