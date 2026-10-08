#include "application.hpp"
#include "scene_math.hpp"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>

namespace application {
namespace {
struct Buffer { VkBuffer handle{}; VmaAllocation allocation{}; void* mapped{}; };
struct alignas(16) Uniform { glm::mat4 mvp; glm::vec4 tint; glm::ivec4 options; };
static_assert(sizeof(Uniform)==96);
struct Object {
    glm::vec3 position{0},rotation{10,-25,5},scale{2},tint{1};
    bool gradient=true,animate=false;
    float radius=1.2f,height=.6f,speed=1.f,spin=35.f,phase=0;
    double clock=0;
    Buffer uniform;
    VkDescriptorSet set{};
};
Object object;
Buffer vertexBuffer,indexBuffer;
VkDescriptorSetLayout setLayout{};
VkDescriptorPool descriptorPool{};
VkPipelineLayout pipelineLayout{};
VkPipeline pipeline{};
bool orthographic=false,playing=true;
float fov=50,orthoSize=4.5f,cameraDistance=10,cameraYaw=35,cameraPitch=25;
double previousTime=-1;
void check(VkResult r,const char* what){if(r!=VK_SUCCESS)throw std::runtime_error(what);}
Buffer makeBuffer(VkDeviceSize size,VkBufferUsageFlags usage,const void* data=nullptr){
    auto& c=graphics::internal::context;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size=size;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo alloc{};alloc.usage=VMA_MEMORY_USAGE_AUTO;
    alloc.flags=VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
    Buffer b;VmaAllocationInfo out;
    check(vmaCreateBuffer(c.allocator,&info,&alloc,&b.handle,&b.allocation,&out),"Buffer allocation failed");
    b.mapped=out.pMappedData;
    if(data){std::memcpy(b.mapped,data,size);check(vmaFlushAllocation(c.allocator,b.allocation,0,size),"Buffer flush failed");}
    return b;
}
VkShaderModule loadShader(const char* name){
    std::ifstream f(name,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error(std::string("Cannot open shader: ")+name);
    auto bytes=f.tellg();if(bytes<=0||bytes%4!=0)throw std::runtime_error("Invalid SPIR-V size");
    std::vector<uint32_t> words(static_cast<size_t>(bytes)/4);
    f.seekg(0);if(!f.read(reinterpret_cast<char*>(words.data()),bytes))throw std::runtime_error("Shader read failed");
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize=static_cast<size_t>(bytes);info.pCode=words.data();VkShaderModule module;
    check(vkCreateShaderModule(graphics::internal::context.device,&info,nullptr,&module),"Shader creation failed");
    return module;
}
void makePipeline(){
    auto& c=graphics::internal::context;
    auto vert=loadShader("shaders/cube.vert.spv");VkShaderModule frag{};
    try {
        frag=loadShader("shaders/cube.frag.spv");
        VkPipelineShaderStageCreateInfo stages[2]{};
        for(auto& s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vert;
        stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=frag;
        VkVertexInputBindingDescription binding{0,3*sizeof(float),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attribute{0,0,VK_FORMAT_R32G32B32_SFLOAT,0};
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertex.vertexBindingDescriptionCount=1;vertex.pVertexBindingDescriptions=&binding;
        vertex.vertexAttributeDescriptionCount=1;vertex.pVertexAttributeDescriptions=&attribute;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=1;viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable=VK_TRUE;depth.depthWriteEnable=VK_TRUE;depth.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&attachment;
        const VkDynamicState dynamicStates[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamicStates;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount=2;info.pStages=stages;info.pVertexInputState=&vertex;info.pInputAssemblyState=&assembly;
        info.pViewportState=&viewport;info.pRasterizationState=&raster;info.pMultisampleState=&ms;
        info.pDepthStencilState=&depth;info.pColorBlendState=&blend;info.pDynamicState=&dynamic;
        info.layout=pipelineLayout;info.renderPass=c.render_pass;
        check(vkCreateGraphicsPipelines(c.device,VK_NULL_HANDLE,1,&info,nullptr,&pipeline),"Graphics pipeline creation failed");
    }catch(...){if(frag)vkDestroyShaderModule(c.device,frag,nullptr);vkDestroyShaderModule(c.device,vert,nullptr);throw;}
    vkDestroyShaderModule(c.device,frag,nullptr);vkDestroyShaderModule(c.device,vert,nullptr);
}
void preset(int demo){
    orthographic=demo==2;playing=false;
    fov=50;orthoSize=4.5f;cameraDistance=10;cameraYaw=35;cameraPitch=25;
    auto& o=object;o.position={0,0,0};o.rotation={10,-25,5};o.scale={2,2,2};o.tint={1,1,1};
    o.gradient=true;o.animate=false;o.radius=1.2f;o.height=.6f;o.speed=1;o.spin=35;
    o.phase=0;o.clock=0;
    if(demo==2){o.scale={2.7f,1.2f,1.6f};o.tint={.3f,.75f,1};o.gradient=false;}
    if(demo==3){
        o.animate=true;o.radius=1.8f;o.height=.8f;o.clock=.65;o.spin=45;o.tint={.65f,1,.8f};
    }
}
void destroyBuffer(Buffer& b){if(b.handle)vmaDestroyBuffer(graphics::internal::context.allocator,b.handle,b.allocation);b={};}
}
bool initialize(){
    try {
        auto& c=graphics::internal::context;
        vertexBuffer=makeBuffer(sizeof(scene::vertices),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,scene::vertices.data());
        indexBuffer=makeBuffer(sizeof(scene::indices),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,scene::indices.data());
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};layout.bindingCount=1;layout.pBindings=&binding;
        check(vkCreateDescriptorSetLayout(c.device,&layout,nullptr,&setLayout),"Descriptor layout creation failed");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
        check(vkCreateDescriptorPool(c.device,&pool,nullptr,&descriptorPool),"Descriptor pool creation failed");
        {auto& o=object;
            o.uniform=makeBuffer(sizeof(Uniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};alloc.descriptorPool=descriptorPool;alloc.descriptorSetCount=1;alloc.pSetLayouts=&setLayout;
            check(vkAllocateDescriptorSets(c.device,&alloc,&o.set),"Descriptor set allocation failed");
            VkDescriptorBufferInfo buffer{o.uniform.handle,0,sizeof(Uniform)};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=o.set;write.dstBinding=0;
            write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&buffer;
            vkUpdateDescriptorSets(c.device,1,&write,0,nullptr);
        }
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layoutInfo.setLayoutCount=1;layoutInfo.pSetLayouts=&setLayout;
        check(vkCreatePipelineLayout(c.device,&layoutInfo,nullptr,&pipelineLayout),"Pipeline layout creation failed");
        makePipeline();preset(1);
        if(const char* demo=std::getenv("LAB1_DEMO"))preset(std::clamp(std::atoi(demo),1,3));
        ImGui::StyleColorsDark();ImGui::GetIO().IniFilename=nullptr;
        ImFontConfig font; font.SizePixels=17; ImGui::GetIO().Fonts->AddFontDefault(&font);
        return true;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';shutdown();return false;}
}
void shutdown(){
    auto& c=graphics::internal::context;vkDeviceWaitIdle(c.device);
    if(pipeline)vkDestroyPipeline(c.device,pipeline,nullptr);
    if(pipelineLayout)vkDestroyPipelineLayout(c.device,pipelineLayout,nullptr);
    if(descriptorPool)vkDestroyDescriptorPool(c.device,descriptorPool,nullptr);
    if(setLayout)vkDestroyDescriptorSetLayout(c.device,setLayout,nullptr);
    destroyBuffer(object.uniform);
    destroyBuffer(vertexBuffer);destroyBuffer(indexBuffer);
    pipeline={};pipelineLayout={};descriptorPool={};setLayout={};
}
void update(double time){
    const double dt=previousTime<0?0:std::max(0.,time-previousTime);previousTime=time;
    if(ImGui::IsKeyPressed(ImGuiKey_F1))preset(1);
    if(ImGui::IsKeyPressed(ImGuiKey_F2))preset(2);
    if(ImGui::IsKeyPressed(ImGuiKey_F3))preset(3);
    if(ImGui::IsKeyPressed(ImGuiKey_Space)&&!ImGui::GetIO().WantCaptureKeyboard)playing=!playing;
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({380,ImGui::GetIO().DisplaySize.y});
    ImGui::Begin("Lab 1 | Cube | Variant 1",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("Le Son Luong | M8O-309BV-24");
    ImGui::TextUnformatted("List #13 -> variant #1 (12 variants)");ImGui::Text("%.1f FPS",ImGui::GetIO().Framerate);
    ImGui::SeparatorText("Projection and camera");
    if(ImGui::RadioButton("Perspective",!orthographic))orthographic=false;
    ImGui::SameLine();if(ImGui::RadioButton("Orthographic",orthographic))orthographic=true;
    if(orthographic)ImGui::SliderFloat("Half height",&orthoSize,.5f,10);else ImGui::SliderFloat("FOV",&fov,20,100,"%.0f deg");
    ImGui::SeparatorText("Cube");
    auto& o=object;
    ImGui::DragFloat3("Position",glm::value_ptr(o.position),.02f,-10,10);
    ImGui::SliderFloat3("Rotation XYZ",glm::value_ptr(o.rotation),-180,180,"%.0f deg");
    ImGui::SliderFloat3("Scale XYZ",glm::value_ptr(o.scale),.1f,4);
    ImGui::ColorEdit3("Tint",glm::value_ptr(o.tint));ImGui::Checkbox("Local vertex colors",&o.gradient);
    ImGui::SeparatorText("Animation");ImGui::Checkbox("Animate cube",&o.animate);ImGui::Checkbox("Play / Pause",&playing);
    ImGui::SliderFloat("Speed",&o.speed,0,3);ImGui::SliderFloat("Radius",&o.radius,0,4);
    ImGui::SliderFloat("Vertical amplitude",&o.height,0,2);ImGui::SliderFloat("Spin",&o.spin,-180,180,"%.0f deg/s");
    if(ImGui::Button("Restart animation"))o.clock=0;
    ImGui::TextUnformatted("x=R cos(t), z=R sin(t), y=H sin(2t)");
    ImGui::SeparatorText("Demonstrations");
    if(ImGui::Button("1 Perspective (F1)"))preset(1);
    if(ImGui::Button("2 Orthographic (F2)"))preset(2);
    if(ImGui::Button("3 Animation (F3)"))preset(3);
    ImGui::End();
    // Simulation time is frozen while paused, so resuming does not jump.
    if(playing&&o.animate)o.clock+=dt*o.speed;
}
void render(const graphics::internal::FrameData& fd){
    if(!fd.command_buffer)return;
    auto& c=graphics::internal::context;
    // prepare() waits for the previous frame fence before mapped UBOs are written.
    const float left=std::min(380.f,float(c.swapchain_extent.width)*.4f);
    const float w=std::max(1.f,float(c.swapchain_extent.width)-left),h=float(c.swapchain_extent.height);
    const float yaw=glm::radians(cameraYaw),pitch=glm::radians(cameraPitch);
    glm::vec3 eye=cameraDistance*glm::vec3(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch));
    auto vp=scene::projection(orthographic,w/h,fov,orthoSize)*glm::lookAtRH(eye,glm::vec3(0),glm::vec3(0,1,0));
    {auto& o=object;
        auto pos=o.position,rot=o.rotation;
        if(o.animate){pos+=scene::path(float(o.clock),o.radius,o.height,o.phase);rot+=glm::vec3(15,35,20)*float(o.clock)*o.spin/35.f;}
        Uniform u{vp*scene::model(pos,rot,o.scale),glm::vec4(o.tint,1),glm::ivec4(o.gradient?1:0,0,0,0)};
        std::memcpy(o.uniform.mapped,&u,sizeof(u));check(vmaFlushAllocation(c.allocator,o.uniform.allocation,0,sizeof(u)),"Uniform flush failed");
    }
    check(vkResetCommandBuffer(fd.command_buffer,0),"Command buffer reset failed");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(fd.command_buffer,&begin),"Command buffer begin failed");
    VkClearValue clear[2]{};clear[0].color={{.045f,.065f,.10f,1}};clear[1].depthStencil={1,0};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};pass.renderPass=c.render_pass;
    pass.framebuffer=fd.framebuffer;pass.renderArea.extent=c.swapchain_extent;pass.clearValueCount=2;pass.pClearValues=clear;
    vkCmdBeginRenderPass(fd.command_buffer,&pass,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{left,0,w,h,0,1};VkRect2D scissor{{int32_t(left),0},{uint32_t(w),c.swapchain_extent.height}};
    vkCmdSetViewport(fd.command_buffer,0,1,&viewport);vkCmdSetScissor(fd.command_buffer,0,1,&scissor);
    vkCmdBindPipeline(fd.command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    VkDeviceSize offset=0;vkCmdBindVertexBuffers(fd.command_buffer,0,1,&vertexBuffer.handle,&offset);vkCmdBindIndexBuffer(fd.command_buffer,indexBuffer.handle,0,VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(fd.command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&object.set,0,nullptr);
    vkCmdDrawIndexed(fd.command_buffer,uint32_t(scene::indices.size()),1,0,0,0);
    vkCmdEndRenderPass(fd.command_buffer);check(vkEndCommandBuffer(fd.command_buffer),"Command buffer end failed");
}
} // namespace application
