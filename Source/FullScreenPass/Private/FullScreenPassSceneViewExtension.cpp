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

static TAutoConsoleVariable<int32> CVarUseSharpen(
	TEXT("r.FSP.UseSharpen"),
	1,
	TEXT("0 = Use existing shader\n")
	TEXT("1 = Use sharpen shader (default)\n"));

static TAutoConsoleVariable<float> CVarSharpenStrength(
	TEXT("r.FSP.SharpenStrength"),
	0.7f,
	TEXT("Sharpen Strength (0-1)\n"));

static TAutoConsoleVariable<int32> CVarDepthMaskEnable(
	TEXT("r.FSP.DepthMaskEnable"),
	1,
	TEXT("Enable Depth Mask (0=disable, 1=enable)\n"));

static TAutoConsoleVariable<int32> CVarRMSMaskEnable(
	TEXT("r.FSP.RMSMaskEnable"),
	1,
	TEXT("Enable RMS Local Contrast Mask (0=disable, 1=enable)\n"));

static TAutoConsoleVariable<int32> CVarSharpenMode(
	TEXT("r.FSP.SharpenMode"),
	1,
	TEXT("Sharpen Mode (0=Chroma, 1=Luma)\n"));

static TAutoConsoleVariable<float> CVarEffectStrenght(
	TEXT("r.FSP.EffectStrenght"),
	0.1f,
	TEXT("Controls an amount of depth buffer blending to the base color."));


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

	FRDGTexture* ResultTexture = GraphBuilder.CreateTexture(SceneColorDesc, TEXT("FulllScreenPassResult"));
	FScreenPassRenderTarget ResultRenderTarget = FScreenPassRenderTarget(ResultTexture, SceneColor.ViewRect, ERenderTargetLoadAction::EClear);
	
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	if (CVarUseSharpen->GetInt() == 1)
	{
		// ========== USE SHARPEN SHADER ==========
		
		// Get sharpen shaders
		TShaderMapRef<FSharpenPassVS> VertexShader(GlobalShaderMap);
		TShaderMapRef<FSharpenPassPS> PixelShader(GlobalShaderMap);
		
		// Allocate parameters for sharpen shader
		FSharpenPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FSharpenPassPS::FParameters>();
		
		// Set common parameters
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneTexturesStruct = Inputs.SceneTextures;
		
		// Set sharpen-specific parameters
		Parameters->SharpenStrength = CVarSharpenStrength->GetFloat();
		Parameters->DepthMaskEnable = CVarDepthMaskEnable->GetInt() > 0 ? 1 : 0;
		Parameters->RMSMaskEnable = CVarRMSMaskEnable->GetInt() > 0 ? 1 : 0;
		Parameters->SharpenMode = CVarSharpenMode->GetInt();
		
		// Set render target
		Parameters->RenderTargets[0] = ResultRenderTarget.GetRenderTargetBinding();

		// Draw the screen pass with sharpen shaders
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("SharpenPass"),
			View,
			Viewport,
			Viewport,
			VertexShader,
			PixelShader,
			Parameters
		);
	}
	else
	{
		// ========== USE EXISTING SHADER ==========
		
		// Get existing shaders
		TShaderMapRef<FFullScreenPassVS> VertexShader(GlobalShaderMap);
		TShaderMapRef<FFullScreenPassPS> PixelShader(GlobalShaderMap);
		
		// Allocate parameters for existing shader
		FFullScreenPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FFullScreenPassPS::FParameters>();
		
		// Set parameters
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneTexturesStruct = Inputs.SceneTextures;
		Parameters->Strenght = CVarEffectStrenght->GetFloat();  // Your existing parameter
		
		// Set render target
		Parameters->RenderTargets[0] = ResultRenderTarget.GetRenderTargetBinding();

		// Draw the screen pass with existing shaders
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("ExistingPass"),
			View,
			Viewport,
			Viewport,
			VertexShader,
			PixelShader,
			Parameters
		);
	}

	// Copy result back to scene color
	AddCopyTexturePass(GraphBuilder, ResultTexture, SceneColor.Texture);
}