#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>
#include <random>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>

static float get_random_float()
{
  static std::random_device dev;
  static std::mt19937 generator( dev() );
  std::uniform_real_distribution<float> uniform(0.f, 1.f);
  return uniform(generator);
}

static float halton_sequence(int i)
{
  float f = 1, r = 0;
  while (i > 0)
  {
    f = f / 2;
    r = r + f * (i % 2);
    i = i / 2;
  }
  return r;
}

static float2 get_halton_float2()
{
  static int i = 0;
  return {halton_sequence(i++), halton_sequence(i++)};
}

/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  gBuffer.mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  gBuffer.shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{4096, 4096, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  gBuffer.position = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "gbuffer_position",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  gBuffer.normal = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "gbuffer_normal",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  gBuffer.albedo = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "gbuffer_albedo",
    .format = vk::Format::eR8G8B8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  gBuffer.ssao = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "ssao_tex",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  gBuffer.blurredSsao = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "blurred_ssao_tex",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  gBuffer.lightViewWposition = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{4096, 4096, 1},
    .name = "gbuffer_light_view_world_position",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  gBuffer.lightViewWnormal = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{4096, 4096, 1},
    .name = "gbuffer_light_view_world_normal",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  gBuffer.flux = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{4096, 4096, 1},
    .name = "rsm_flux",
    .format = vk::Format::eR8G8B8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  hdrImage = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "hdr_image",
    .format = vk::Format::eR16G16B16A16Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.addressMode = vk::SamplerAddressMode::eClampToBorder, .name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  ssaoSamples = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float4) * m_uniforms.ssaoKernelSize,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "ssao_samples"
  });

  ssaoNoise = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float4) * m_uniforms.ssaoNoiseSize * m_uniforms.ssaoNoiseSize,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "ssao_noise"
  });

  rsmSamples = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(float2) * 128,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "rsm_samples"
  });
  

  m_uboMappedMem = constants.map();

  void *ssaoSamplesMappedMem = ssaoSamples.map();
  std::vector<float4> ssaoSamplesVec;
  ssaoSamplesVec.reserve(m_uniforms.ssaoKernelSize);
  for (uint32_t i = 0; i < m_uniforms.ssaoKernelSize; ++i)
  {
    float4 sample = {get_random_float() * 2.f - 1.f, get_random_float() * 2.f - 1.f, get_random_float(), 0.0};
    ssaoSamplesVec.push_back(LiteMath::normalize(sample) * get_random_float());
  }
  memcpy(ssaoSamplesMappedMem, ssaoSamplesVec.data(), ssaoSamplesVec.size() * sizeof(float4));
  ssaoSamples.unmap();

  void *ssaoNoiseMappedMem = ssaoNoise.map();
  std::vector<float4> ssaoNoiseVec(m_uniforms.ssaoNoiseSize*m_uniforms.ssaoNoiseSize,
                                   {get_random_float() * 2.f - 1.f, get_random_float() * 2.f - 1.f, 0.f, 0.0f});
  memcpy(ssaoNoiseMappedMem, ssaoNoiseVec.data(), m_uniforms.ssaoNoiseSize*m_uniforms.ssaoNoiseSize*sizeof(float4));
  ssaoNoise.unmap();
  
  m_gaussian_kernel.resize(m_gaussian_kernel_length);
  float kernel_radius = (static_cast<float>(m_gaussian_kernel_length) - 1.f) / 2.f;
  float sigma = kernel_radius / 3.f;
  float doubled_sigma2 = 2.f * sigma * sigma;
  float sum = 0.f;

  for (uint32_t i = 0; i < m_gaussian_kernel_length; i++) {
    float delta = float(i) - kernel_radius;
    m_gaussian_kernel[i] = std::exp(- delta * delta / doubled_sigma2);
    sum += m_gaussian_kernel[i];
  }  
  for (uint32_t i = 0; i < m_gaussian_kernel_length; i++) {
    m_gaussian_kernel[i] /= sum;
  }

  void* rsmSamplesMappedMem = rsmSamples.map();
  auto rsmSamplesVec = std::vector<float2>();
  rsmSamplesVec.reserve(400);
  for (size_t i = 0; i < 400; i++)
    rsmSamplesVec.push_back(get_halton_float2());

  memcpy(rsmSamplesMappedMem, rsmSamplesVec.data(), rsmSamplesVec.size());
  rsmSamples.unmap();
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;
}

void SimpleShadowmapRender::DeallocateResources()
{
  gBuffer.mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  gBuffer.shadowMap.reset();
  gBuffer.position.reset();
  gBuffer.normal.reset();
  gBuffer.albedo.reset();
  gBuffer.ssao.reset();
  gBuffer.blurredSsao.reset();
  gBuffer.lightViewWposition.reset();
  gBuffer.lightViewWnormal.reset();
  gBuffer.flux.reset();
  hdrImage.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
  ssaoSamples = etna::Buffer();
  ssaoNoise = etna::Buffer();
  rsmSamples = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pFSQuad = std::make_shared<vk_utils::QuadRenderer>(0,0, 512, 512);
  m_pFSQuad->Create(m_context->getDevice(),
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv",
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad.frag.spv",
    vk_utils::RenderTargetInfo2D{
      .size          = VkExtent2D{ m_width, m_height },// this is debug full screen quad
      .format        = m_swapchain.GetFormat(),
      .loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,// seems we need LOAD_OP_LOAD if we want to draw quad to part of screen
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
    }
  );
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_material",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_shadow",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/flux.frag.spv"});
  etna::create_program("prepare_gbuffer",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/prepare_gbuffer.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("resolve_gbuffer",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/resolve_gbuffer.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"});
  etna::create_program("calculate_ssao",
    { VK_GRAPHICS_BASIC_ROOT"/resources/shaders/ssao.frag.spv", VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv" });
  etna::create_program("gaussian_blur", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/gaussian_blur.comp.spv"});
  etna::create_program("tonemapping",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/tonemapping.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"});
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     2}
  };

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_context->getDevice(), dtypes, 2);
  
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, hdrImage.getView({}), defaultSampler.get(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);

  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto blendAttachment = vk::PipelineColorBlendAttachmentState
    {
      .blendEnable = false,
      .colorWriteMask = vk::ColorComponentFlagBits::eR
        | vk::ColorComponentFlagBits::eG
        | vk::ColorComponentFlagBits::eB
        | vk::ColorComponentFlagBits::eA
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR16G16B16A16Sfloat},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .blendingConfig = 
        {
          .attachments = {blendAttachment, blendAttachment, blendAttachment}
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32B32A32Sfloat, vk::Format::eR32G32B32A32Sfloat, vk::Format::eR8G8B8A8Srgb},
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_prepareGbufferPipeline = pipelineManager.createGraphicsPipeline("prepare_gbuffer",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .blendingConfig =
        {
          .attachments = {blendAttachment, blendAttachment, blendAttachment}
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32B32A32Sfloat, vk::Format::eR32G32B32A32Sfloat, vk::Format::eR8G8B8A8Srgb},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_resolveGbufferPipeline = pipelineManager.createGraphicsPipeline("resolve_gbuffer",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = { vk::Format::eR16G16B16A16Sfloat },
        }
    });
  m_ssaoPipeline = pipelineManager.createGraphicsPipeline("calculate_ssao",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32Sfloat},
        }
    });
  m_gaussianBlurPipeline = pipelineManager.createComputePipeline("gaussian_blur", {});
  m_tonemappingPipeline = pipelineManager.createGraphicsPipeline("tonemapping",
    {
      .fragmentShaderOutput = { .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())} }
    });
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}



/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp;
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    auto inst         = m_pScnMgr->GetInstanceInfo(i);
    pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
    pushConst2M.colorNo = i;
    vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  //// draw scene to shadowmap
  //
  {
    etna::RenderTargetState renderTargets(a_cmdBuff, {4096, 4096}, {
        {gBuffer.lightViewWposition}, {gBuffer.lightViewWnormal}, {gBuffer.flux}
      }, gBuffer.shadowMap);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
    DrawSceneCmd(a_cmdBuff, m_lightMatrix);
  }

  //// prepare gbuffer
  //
  {
    auto prepareGbufferInfo = etna::get_shader_program("prepare_gbuffer");
    auto set = etna::create_descriptor_set(prepareGbufferInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{gBuffer.position}, {gBuffer.normal}, {gBuffer.albedo}}, gBuffer.mainViewDepth);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_prepareGbufferPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_prepareGbufferPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj);
  }
  //// calculate SSAO
  //
  {
    auto ssaoInfo = etna::get_shader_program("calculate_ssao");
    auto set = etna::create_descriptor_set(ssaoInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, gBuffer.position.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, gBuffer.normal.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {3, ssaoSamples.genBinding()},
      etna::Binding {4, ssaoNoise.genBinding()}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{gBuffer.ssao}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_ssaoPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
  }

  //// blur SSAO texture
  //
  if (m_uniforms.ssaoEnabled)
  {
    auto gaussianBlurInfo = etna::get_shader_program("gaussian_blur");
    auto set = etna::create_descriptor_set(gaussianBlurInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, gBuffer.ssao.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
      etna::Binding {1, gBuffer.blurredSsao.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
      etna::Binding {2, gBuffer.position.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
    });
    VkDescriptorSet vkSet = set.getVkSet();
    etna::flush_barriers(a_cmdBuff);

    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_COMPUTE_BIT);
    vkCmdPushConstants(a_cmdBuff, m_gaussianBlurPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(float) * m_gaussian_kernel_length, m_gaussian_kernel.data());

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_gaussianBlurPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_gaussianBlurPipeline.getVkPipeline());
    vkCmdDispatch(a_cmdBuff, m_width / 32 + 1, m_height / 32 + 1, 1);
  }

  //// resolve gbuffer
  //
  {

    auto resolveGbufferInfo = etna::get_shader_program("resolve_gbuffer");
    auto set = etna::create_descriptor_set(resolveGbufferInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, gBuffer.shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, gBuffer.position.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {3, gBuffer.normal.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {4, gBuffer.albedo.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {5, gBuffer.blurredSsao.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {6, gBuffer.lightViewWposition.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {7, gBuffer.lightViewWnormal.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {8, gBuffer.flux.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {9, rsmSamples.genBinding()},
    });
    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{hdrImage}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolveGbufferPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_resolveGbufferPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
  }

    etna::set_state(a_cmdBuff, hdrImage.get(), vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::flush_barriers(a_cmdBuff);
  {
    auto outputInfo = etna::get_shader_program("tonemapping");
    auto set = etna::create_descriptor_set(outputInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, hdrImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });
    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImage, a_targetImageView}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tonemappingPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_tonemappingPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);
  }

  if(m_input.drawFSQuad)
  {
    float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_quadDS, scaleAndOffset);
  }

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
