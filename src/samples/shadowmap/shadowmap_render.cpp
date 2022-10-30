#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>

#include <random>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY
  });
  m_uboMappedMem = constants.map();

  positionMatrices = m_context->createBuffer(etna::Buffer::CreateInfo 
  {
    .size = sizeof(LiteMath::float4x4) * m_numInstances,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY
  });
  m_uboPositionMatrices  = positionMatrices.map();

  instanceCount = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(uint32_t),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY
  });
  m_uboInstanceCount = instanceCount.map();

  outputInstances = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(uint32_t) * m_numInstances,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY
  });
  m_uboOutputInstances = outputInstances.map();

  infoIndirect = m_context->createBuffer(etna::Buffer::CreateInfo{
    .size        = sizeof(VkDrawIndexedIndirectCommand),
    .bufferUsage = vk::BufferUsageFlagBits::eIndirectBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU });
  m_sInfoIndirect    = infoIndirect.map();
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
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();

  constants = etna::Buffer();
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
    vk_utils::RenderTargetInfo2D{ VkExtent2D{ m_width, m_height }, m_swapchain.GetFormat(),                                        // this is debug full scree quad
                                                  VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR }); // seems we need LOAD_OP_LOAD if we want to draw quad to part of screen

  SetupSimplePipeline();
}

static void print_prog_info(const std::string &name)
{
  auto info = etna::get_shader_program(name);
  std::cout << "Program Info " << name << "\n";

  for (uint32_t set = 0u; set < etna::MAX_PROGRAM_DESCRIPTORS; set++)
  {
    if (!info.isDescriptorSetUsed(set))
      continue;
    auto setInfo = info.getDescriptorSetInfo(set);
    for (uint32_t binding = 0; binding < etna::MAX_DESCRIPTOR_BINDINGS; binding++)
    {
      if (!setInfo.isBindingUsed(binding))
        continue;
      auto &vkBinding = setInfo.getBinding(binding);

      std::cout << "Binding " << binding << " " << vk::to_string(vkBinding.descriptorType) << ", count = " << vkBinding.descriptorCount << " ";
      std::cout << " " << vk::to_string(vkBinding.stageFlags) << "\n"; 
    }
  }

  auto pc = info.getPushConst();
  if (pc.size)
  {
    std::cout << "PushConst " << " size = " << pc.size << " stages = " << vk::to_string(pc.stageFlags) << "\n";
  }
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("frustum_culling", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/frustum_culling.comp.spv"});
  etna::create_program("simple_material", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_shadow", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     2},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             3}
  };

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_context->getDevice(), dtypes, 2);
  
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, shadowMap.getView({}), defaultSampler.get(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);
  
  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, positionMatrices.get());
  m_pBindings->BindBuffer(1, instanceCount.get());
  m_pBindings->BindBuffer(2, outputInstances.get());
  m_pBindings->BindEnd(&m_computeDS, &m_computeDSLayout);

  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_cullingPipeline      = pipelineManager.createComputePipeline("frustum_culling", {});

  
  print_prog_info("frustum_culling");
  print_prog_info("simple_material");
  print_prog_info("simple_shadow");
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}



/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::CreateInstances()
{
  std::mt19937 generator;
  generator.seed(40);
  std::uniform_real_distribution<float> uniform(-800.f, 800.f);
  std::vector<LiteMath::float4x4> instances;
  instances.reserve(m_numInstances);

  for (size_t i = 1; i <= m_numInstances; i++) {
    instances.emplace_back(
      LiteMath::translate4x4({uniform(generator), 0, uniform(generator)})
    );
  }
  memcpy(m_uboPositionMatrices, instances.data(), sizeof(LiteMath::float4x4) * instances.size());
}

void SimpleShadowmapRender::ExecuteComputeShader(VkCommandBuffer a_cmdBuff, const float4x4 &a_wvp)
{
  CreateInstances();
  vkCmdFillBuffer(a_cmdBuff, instanceCount.get(), 0, VK_WHOLE_SIZE, 0);
  vkCmdFillBuffer(a_cmdBuff, outputInstances.get(), 0, VK_WHOLE_SIZE, 0);
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_COMPUTE_BIT);
  pushConstInst.projView = a_wvp;
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    pushConstInst.bbox = m_pScnMgr->GetInstanceBbox(i);
    pushConstInst.instancesCount = m_numInstances;
    vkCmdPushConstants(
      a_cmdBuff, m_cullingPipeline.getVkPipelineLayout(), stageFlags, 0, sizeof(pushConstInst), &pushConstInst
    );
    vkCmdDispatch(a_cmdBuff, pushConstInst.instancesCount / m_workGroupSize + 1, 1, 1);
    {
      std::array barriers{
        // Transfer the shadowmap to depth write layout
        VkBufferMemoryBarrier2{
          .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
          .dstStageMask  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
          .buffer        = instanceCount.get(),
          .size          = sizeof(int32_t)
        }
      };
      VkDependencyInfo depInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags         = VK_DEPENDENCY_BY_REGION_BIT,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pBufferMemoryBarriers   = barriers.data(),
      };
      vkCmdPipelineBarrier2(a_cmdBuff, &depInfo);
    }
  }
}

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);
    VkDeviceSize zero_offset = 0u;
    VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
    VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
    VkDrawIndexedIndirectCommand *indirectBufferMem = (VkDrawIndexedIndirectCommand *)m_sInfoIndirect;

    vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
    vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

    pushConst2M.projView = a_wvp;
    pushConst2M.model = m_pScnMgr->GetInstanceMatrix(0);

    auto mesh_info                     = m_pScnMgr->GetMeshInfo(m_pScnMgr->GetInstanceInfo(0).mesh_id);
    indirectBufferMem->firstIndex      = mesh_info.m_indexOffset;
    indirectBufferMem->firstInstance   = 0;
    indirectBufferMem->indexCount      = mesh_info.m_indNum;
    indirectBufferMem->instanceCount   = *(uint*)m_uboInstanceCount;
    indirectBufferMem->vertexOffset    = mesh_info.m_vertexOffset;

    vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    vkCmdDrawIndexedIndirect(a_cmdBuff, infoIndirect.get(), 0, 1, 0);
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  {
    std::array barriers
      {
        // Transfer the shadowmap to depth write layout
        VkImageMemoryBarrier2
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
          .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = shadowMap.get(),
          .subresourceRange =
            {
              .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            }
        },
      };
    VkDependencyInfo depInfo
      {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
      };
    vkCmdPipelineBarrier2(a_cmdBuff, &depInfo);
  }


  //// draw scene to shadowmap
  //
  {
    etna::RenderTargetState renderTargets(a_cmdBuff, { 2048, 2048 }, {}, shadowMap.getView({}));
    {
      auto simpleShadowInfo = etna::get_shader_program("simple_shadow");
      auto set = etna::create_descriptor_set(simpleShadowInfo.getDescriptorLayoutId(0), { 
        etna::Binding{ 2, vk::DescriptorBufferInfo{ positionMatrices.get(), 0, VK_WHOLE_SIZE } },
        etna::Binding{ 3, vk::DescriptorBufferInfo{ outputInstances.get(), 0, VK_WHOLE_SIZE }}
      });
      VkDescriptorSet vkSet = set.getVkSet();
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
      vkCmdBindDescriptorSets(
        a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipelineLayout(),
        0, 1, &vkSet, 0, VK_NULL_HANDLE
      );
      DrawSceneCmd(a_cmdBuff, m_lightMatrix);
    }
  }

  {
    std::array barriers
      {
        // Transfer the shadowmap from depth write to shader read 
        VkImageMemoryBarrier2
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
          .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = shadowMap.get(),
          .subresourceRange =
            {
              .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            }
        },
        // Wait for the semaphore to signal that the swapchain image is available
        VkImageMemoryBarrier2
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          // Our semo signals this stage
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = 0,
          .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = a_targetImage,
          .subresourceRange =
            {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            }
        },
      };
    VkDependencyInfo depInfo
      {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
      };
    vkCmdPipelineBarrier2(a_cmdBuff, &depInfo);
  }
  //// call compute shader
  {
    auto cullingInfo = etna::get_shader_program("frustum_culling");
    auto set = etna::create_descriptor_set(cullingInfo.getDescriptorLayoutId(0), { 
      etna::Binding{ 0, vk::DescriptorBufferInfo{ positionMatrices.get(), 0, VK_WHOLE_SIZE } }, 
      etna::Binding{ 1, vk::DescriptorBufferInfo{ instanceCount.get(), 0, VK_WHOLE_SIZE } }, 
      etna::Binding{ 2, vk::DescriptorBufferInfo{ outputInstances.get(), 0, VK_WHOLE_SIZE } }, 
    });
    VkDescriptorSet vkSet = set.getVkSet();
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_cullingPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, 
      m_cullingPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    ExecuteComputeShader(a_cmdBuff, m_worldViewProj);
  }
  //// draw final scene to screen
  //
  {
    auto simpleMaterialInfo = etna::get_shader_program("simple_material");

    auto set = etna::create_descriptor_set(simpleMaterialInfo.getDescriptorLayoutId(0), {
      etna::Binding {0, vk::DescriptorBufferInfo {constants.get(), 0, VK_WHOLE_SIZE}},
      etna::Binding {1, vk::DescriptorImageInfo {defaultSampler.get(), shadowMap.getView({}), vk::ImageLayout::eShaderReadOnlyOptimal}},
      etna::Binding {2, vk::DescriptorBufferInfo {positionMatrices.get(), 0, VK_WHOLE_SIZE}},
      etna::Binding {3, vk::DescriptorBufferInfo {outputInstances.get(), 0, VK_WHOLE_SIZE}}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImageView}}, mainViewDepth.getView({}));

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_basicForwardPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj);
  }

  if(m_input.drawFSQuad)
  {
    float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_quadDS, scaleAndOffset);
  }
  
  {
    std::array barriers
      {
        // Transfer swapchain to present layout
        VkImageMemoryBarrier2
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          .dstAccessMask = 0,
          .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = a_targetImage,
          .subresourceRange =
            {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            }
        },
      };
    VkDependencyInfo depInfo
      {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
      };
    vkCmdPipelineBarrier2(a_cmdBuff, &depInfo);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}