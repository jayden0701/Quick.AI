// SPDX-License-Identifier: Apache-2.0
/**
 * @file   model_descriptors_public.cpp
 * @brief  Public model descriptor self-registration. Proprietary model
 *         plugins register themselves in their own TUs, not here.
 *
 * config_name values verified against get_model_name_from_type() in
 * quick_dot_ai_api.cpp.  arch_string values verified against
 * register_models() Factory registrations in the same file.
 */
#include "model_descriptor.h"

using namespace quick_dot_ai;

#define B(x) (1u << (unsigned)(x)) /* BackendType: CPU=0, GPU=1, NPU=2 */

__attribute__((constructor)) static void register_public_descriptors() {
  static const ModelDescriptor kPublic[] = {
    {"qwen3-0.6b", "qwen3-0.6b", "Qwen3 0.6B", QDA_RUNTIME_NATIVE, B(0) | B(1),
     QDA_CAP_STREAMING | QDA_CAP_TOOL_USE,
     "QWEN3-0.6B", /* get_model_name_from_type(CAUSAL_LM_MODEL_QWEN3_0_6B) */
     "Qwen3ForCausalLM", QDA_ROLE_TEXT_LLM, 0, ""},
    {"qwen3-1.7b-q40", "qwen3-1.7b", "Qwen3 1.7B (Q40)", QDA_RUNTIME_NATIVE,
     B(0) | B(1), QDA_CAP_STREAMING | QDA_CAP_TOOL_USE,
     "QWEN3-1.7B-Q40", /* get_model_name_from_type(CAUSAL_LM_MODEL_QWEN3_1_7B_Q40)
                        */
     "Qwen3ForCausalLM", QDA_ROLE_TEXT_LLM, 0, ""},
    {"tiny-bert", "tiny-bert", "Tiny BERT", QDA_RUNTIME_NATIVE, B(0),
     QDA_CAP_EMBEDDING,
     "TINY_BERT", /* get_model_name_from_type(CAUSAL_LM_MODEL_TINY_BERT) */
     "MultilingualTinyBert", QDA_ROLE_UNKNOWN, 0, ""},
    {"function-gemma", "function-gemma", "Function Gemma", QDA_RUNTIME_NATIVE,
     B(0) | B(1), QDA_CAP_TOOL_USE,
     "FUNCTION_GEMMA", /* get_model_name_from_type(CAUSAL_LM_MODEL_FUNCTION_GEMMA)
                        */
     "Gemma3ForCausalLM", QDA_ROLE_TEXT_LLM, 0, ""},
    {"gemma4-cpu", "gemma4", "Gemma4 (CPU)", QDA_RUNTIME_NATIVE, B(0),
     QDA_CAP_STREAMING,
     "GEMMA4_CPU", /* get_model_name_from_type(CAUSAL_LM_MODEL_GEMMA4_CPU) */
     "Gemma4ForCausalLM" /* Factory registration pending */, QDA_ROLE_TEXT_LLM,
     0, ""},
    {"lfm2-siglip-llm", "lfm2", "LFM2 for SigLIP", QDA_RUNTIME_NATIVE,
     B(0) | B(1), 0, nullptr, "Lfm2ForCausalLM", QDA_ROLE_TEXT_LLM, 1024,
     "siglip-lfm2-vision,lfm2-siglip-connector,lfm2-siglip"},
    {"siglip-lfm2-vision", "siglip", "SigLIP for LFM2", QDA_RUNTIME_NATIVE,
     B(0) | B(1), QDA_CAP_VISION_ENCODER, nullptr, "Lfm2VlVisionTransformer",
     QDA_ROLE_VISION_ENCODER, 768,
     "lfm2-siglip-llm,lfm2-siglip-connector,lfm2-siglip"},
    {"lfm2-siglip-connector", "lfm2", "LFM2 SigLIP Connector",
     QDA_RUNTIME_NATIVE, B(0), 0, nullptr, nullptr, QDA_ROLE_CONNECTOR, 1024,
     "lfm2-siglip-llm,siglip-lfm2-vision,lfm2-siglip"},
    {"lfm2-siglip", "lfm2", "LFM2 + SigLIP", QDA_RUNTIME_NATIVE,
     B(0) | B(1), 0, nullptr, nullptr, QDA_ROLE_COMPOSITION, 1024,
     "lfm2-siglip-llm,siglip-lfm2-vision,lfm2-siglip-connector"},
#ifdef ENABLE_QNN
    {"gemma4-e2b-qnn", "gemma4", "Gemma4 E2B (QNN)", QDA_RUNTIME_NATIVE, B(2),
     QDA_CAP_MESSAGES_API,
     "GEMMA4-E2B-QNN", /* get_model_name_from_type(CAUSAL_LM_MODEL_GEMMA4_E2B_QNN)
                         */
     "Gemma4_E2B_QNN", QDA_ROLE_TEXT_LLM, 0, ""},
    {"vjepa-qnn", "vjepa", "V-JEPA (QNN)", QDA_RUNTIME_NATIVE, B(2),
     QDA_CAP_MULTIMODAL | QDA_CAP_MESSAGES_API | QDA_CAP_MULTI_IMAGE,
     "VJEPA-QNN", /* get_model_name_from_type(CAUSAL_LM_MODEL_VJEPA_QNN) */
     "VJEPA_QNN", QDA_ROLE_VISION_ENCODER, 0, ""},
    {"lfm2-jepa-llm", "lfm2", "LFM2 for JEPA", QDA_RUNTIME_NATIVE, B(0), 0,
     nullptr, "Lfm2ForCausalLM", QDA_ROLE_TEXT_LLM, 1024,
     "jepa-qnn-vision,lfm2-jepa-connector,lfm2-jepa"},
    {"jepa-qnn-vision", "jepa", "JEPA Vision (QNN)", QDA_RUNTIME_NATIVE, B(2),
     QDA_CAP_VISION_ENCODER | QDA_CAP_MULTI_IMAGE, nullptr, "VJEPA_QNN",
     QDA_ROLE_VISION_ENCODER, 0,
     "lfm2-jepa-llm,lfm2-jepa-connector,lfm2-jepa"},
    {"lfm2-jepa-connector", "lfm2", "LFM2 JEPA Connector", QDA_RUNTIME_NATIVE,
     B(0), 0, nullptr, nullptr, QDA_ROLE_CONNECTOR, 1024,
     "lfm2-jepa-llm,jepa-qnn-vision,lfm2-jepa"},
    {"lfm2-jepa", "lfm2", "LFM2 + JEPA", QDA_RUNTIME_NATIVE, B(0) | B(2), 0,
     nullptr, nullptr, QDA_ROLE_COMPOSITION, 1024,
     "lfm2-jepa-llm,jepa-qnn-vision,lfm2-jepa-connector"},
#endif
  };
  for (const auto &d : kPublic)
    register_model_descriptor(&d);
}
