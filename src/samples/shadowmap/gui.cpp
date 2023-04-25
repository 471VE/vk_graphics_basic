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
    
    ImGui::Checkbox("SSAO", (bool*)&m_uniforms.ssaoEnabled);
    static const std::array modeNames{"None", "Reinhard", "Reinhard-Jodie", "Uncharted 2 Filmic", "ACES", "Approximate ACES"};
    uint32_t prevMode = m_uniforms.tonemappingMode;
    ImGui::Combo("Tonemapping", reinterpret_cast<int*>(&m_uniforms.tonemappingMode), modeNames.data(), static_cast<int>(modeNames.size()));
    ImGui::SliderFloat("Light Intensity", &m_uniforms.lightIntensity, 0.f, 5.f);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
