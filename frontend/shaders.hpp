#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <volk.h>

namespace iris {

struct Instance;

namespace shaders {
    class Pass {
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        VkRenderPass m_render_pass = VK_NULL_HANDLE;
        VkImageView m_input = VK_NULL_HANDLE;
        VkShaderModule m_vert_shader = VK_NULL_HANDLE;
        VkShaderModule m_frag_shader = VK_NULL_HANDLE;
        Instance* m_iris = nullptr;
        std::string m_id;

    public:
        void destroy();
        bool init(Instance* iris, const void* data, size_t size, std::string id);

        void swap(Pass& rhs) {
            VkPipelineLayout pipeline_layout = m_pipeline_layout;
            VkPipeline pipeline = m_pipeline;
            VkRenderPass render_pass = m_render_pass;
            VkImageView input = m_input;
            VkShaderModule vert_shader = m_vert_shader;
            VkShaderModule frag_shader = m_frag_shader;
            Instance* iris = m_iris;
            std::string id = m_id;

            m_pipeline_layout = rhs.m_pipeline_layout;
            m_pipeline = rhs.m_pipeline;
            m_render_pass = rhs.m_render_pass;
            m_input = rhs.m_input;
            m_vert_shader = rhs.m_vert_shader;
            m_frag_shader = rhs.m_frag_shader;
            m_iris = rhs.m_iris;
            m_id = rhs.m_id;

            rhs.m_pipeline_layout = pipeline_layout;
            rhs.m_pipeline = pipeline;
            rhs.m_render_pass = render_pass;
            rhs.m_input = input;
            rhs.m_vert_shader = vert_shader;
            rhs.m_frag_shader = frag_shader;
            rhs.m_iris = iris;
            rhs.m_id = id;
        }

        Pass(Instance* iris, const void* data, size_t size, std::string id);
        Pass(Pass&& other);
        Pass() = default;
        ~Pass();

        Pass& operator=(Pass&& other);

        VkPipelineLayout& get_pipeline_layout();
        VkPipeline& get_pipeline();
        VkRenderPass& get_render_pass();
        VkImageView& get_input();
        VkShaderModule& get_vert_shader();
        VkShaderModule& get_frag_shader();
        std::string get_id() const;

        bool bypass = false;
        bool ready();
        bool rebuild();
    };

    void push(Instance* iris, void* data, size_t size, std::string id);
    void push(Instance* iris, std::string id);
    void pop(Instance* iris);
    void insert(Instance* iris, int i, void* data, size_t size, std::string id);
    void insert(Instance* iris, std::string id);
    void erase(Instance* iris, int i);
    Pass* at(Instance* iris, int i);
    void swap(Instance* iris, int i1, int i2);
    Pass* front(Instance* iris);
    Pass* back(Instance* iris);
    size_t count(Instance* iris);
    void clear(Instance* iris);
    std::vector <shaders::Pass*>& vector(Instance* iris);
}

}
