#include <iostream>

#include <vkpt/context.h>

using namespace vkpt;

void run()
{
    Context context(Context::Description{
        .width       = 640,
        .height      = 480,
        .title       = "01.deferred",
        .image_count = 3
    });

    auto frame_resources = context.createFrameResources();
    auto imgui = context.getImGuiIntegration();

    AGZ_SCOPE_GUARD({ context.waitIdle(); });

    while(!context.getCloseFlag())
    {
        context.doEvents();
        if(context.isMinimized() || !context.acquireNextImage())
            continue;

        if(context.getInput()->isDown(KEY_ESCAPE))
            context.setCloseFlag(true);

        frame_resources.beginFrame();
        imgui->newFrame();

        rg::Graph graph;

        auto acquire_pass = context.addAcquireImagePass(graph);
        auto present_pass = context.addPresentImagePass(graph);

        auto imgui_pass = imgui->addToGraph(
            context.getImage(), context.getImageView(), graph);

        graph.addDependency(acquire_pass, imgui_pass, present_pass);

        graph.optimize();
        graph.execute(frame_resources);

        context.swapBuffers();
        frame_resources.endFrame(context.getGraphicsQueue());
    }
}

int main()
{
#ifdef VKPT_DEBUG
    run();
#else
    try
    {
        run();
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }
#endif
}
