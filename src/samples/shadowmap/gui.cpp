#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Render settings");
    ImGui::Checkbox("SSAO", (bool*)&m_uniforms.ssaoEnabled);
    static const std::array modeNames{"None", "Reinhard", "Reinhard-Jodie", "Uncharted 2 Filmic", "ACES", "Approximate ACES"};
    ImGui::Combo("Tonemapping", reinterpret_cast<int*>(&m_uniforms.tonemappingMode), modeNames.data(), static_cast<int>(modeNames.size()));
    ImGui::SliderFloat("Light Intensity", &m_uniforms.lightIntensity, 0.f, 5.f);
    ImGui::Checkbox("Direct Illumination", (bool*)&m_uniforms.directIlluminationEnabled);
    ImGui::SameLine(0.f, 30.f);
    ImGui::Checkbox("Indirect Illumination", (bool*)&m_uniforms.indirectIlluminationEnabled);
    ImGui::Checkbox("Subsurface Scattering", (bool*)&m_uniforms.sssEnabled);
    ImGui::SameLine(0.f, 30.f);
    ImGui::RadioButton("For teapot", reinterpret_cast<int*>(&m_uniforms.sssForTeapot), 1); ImGui::SameLine();
    ImGui::RadioButton("For everything", reinterpret_cast<int*>(&m_uniforms.sssForTeapot), 0);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
