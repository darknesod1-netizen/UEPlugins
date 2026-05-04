#include "FullScreenPassSceneViewExtension.h"
#include "FullScreenPassShaders.h"

#include "FXRenderingUtils.h"
#include "PostProcess/PostProcessInputs.h"
#include "DynamicResolutionState.h"


static TAutoConsoleVariable<int32> CVarEnabled(
    TEXT("r.FSP"),
    1,
    TEXT("Controls Full Screen Pass plugin\n")
    TEXT(" 0: disabled\n")
    TEXT(" 1: enabled (default)"));

static TAutoConsoleVariable<float> CVarSharpStrength(
    TEXT("r.FSP.SharpStrength"),
    0.7f,
    TEXT("Sharpen Strength (0.0 - 1.0)"));

static TAutoConsoleVariable<int32> CVarDepthMaskEnable(
    TEXT("r.FSP.DepthMaskEnable"),
    1,
    TEXT("Use Depth Mask\n")
    TEXT(" 0: disabled\n")
    TEXT(" 1: enabled (default)"));

static TAutoConsoleVariable<int32> CVarRmsMaskEnable(
    TEXT("r.FSP.RmsMaskEnable"),
    1,
    TEXT("Use Local Contrast Enhancer (RMS Mask)\n")
    TEXT(" 0: disabled\n")
    TEXT(" 1: enabled (default)"));

static TAutoConsoleVariable<int32> CVarSharpenMode(
    TEXT("r.FSP.SharpenMode"),
    1,
    TEXT("Sharpen Mode\n")
    TEXT(" 0: Chroma\n")
    TEXT(" 1: Luma (default)"));


FFullScreenPassSceneViewExtension::FFullScreenPassSceneViewExtension(const FAutoRegister& AutoRegister) :
    FSceneViewExtensionBase(AutoRegister)
{

}

void FFullScreenPassSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
    if (CVarEnabled->GetInt() == 0)
    {
        return;
    }

    Inputs.Validate();

    const FIntRect PrimaryViewRect = UE::FXRenderingUtils::GetRawViewRectUnsafe(View);

    FScreenPassTexture SceneColor((*Inputs.SceneTextures)->SceneColorTexture, PrimaryViewRect);

    if (!SceneColor.IsValid())
    {
        return;
    }

    const FScreenPassTextureViewport Viewport(SceneColor);

    FRDGTextureDesc SceneColorDesc = SceneColor.Texture->Desc;
    SceneColorDesc.Format = PF_FloatRGBA;
    FLinearColor ClearColor(0., 0., 0., 0.);
    SceneColorDesc.ClearValue = FClearValueBinding(ClearColor);

    FRDGTexture* ResultTexture = GraphBuilder.CreateTexture(SceneColorDesc, TEXT("FullScreenPassResult"));
    FScreenPassRenderTarget ResultRenderTarget = FScreenPassRenderTarget(ResultTexture, SceneColor.ViewRect, ERenderTargetLoadAction::EClear);

    FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    TShaderMapRef<FFullScreenPassVS> ScreenPassVS(GlobalShaderMap);
    TShaderMapRef<FFullScreenPassPS> ScreenPassPS(GlobalShaderMap);

    FFullScreenPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FFullScreenPassPS::FParameters>();
    Parameters->View                = View.ViewUniformBuffer;
    Parameters->SceneTexturesStruct = Inputs.SceneTextures;
    Parameters->SharpStrength       = CVarSharpStrength->GetFloat();
    Parameters->DepthMaskEnable     = CVarDepthMaskEnable->GetInt();
    Parameters->RmsMaskEnable       = CVarRmsMaskEnable->GetInt();
    Parameters->SharpenMode         = CVarSharpenMode->GetInt();
    Parameters->RenderTargets[0]    = ResultRenderTarget.GetRenderTargetBinding();

    AddDrawScreenPass(
        GraphBuilder,
        RDG_EVENT_NAME("FullScreenPassShader"),
        View,
        Viewport,
        Viewport,
        ScreenPassVS,
        ScreenPassPS,
        Parameters
    );

    AddCopyTexturePass(GraphBuilder, ResultTexture, SceneColor.Texture);
}