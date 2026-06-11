package com.example.sampletestapp

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonObject

data class ParsedToolApiJsonRequest(
    val prompt: String,
    val toolName: String,
    val toolSchema: String?,
)

object ToolApiJsonRequest {
    fun parse(rawJson: String): ParsedToolApiJsonRequest {
        val root = Json.parseToJsonElement(rawJson).jsonObject
        val prompt = requiredString(root, "prompt")
        val toolName = optionalString(root, "tool_name")
            ?: optionalString(root, "toolName")
            ?: throw IllegalArgumentException("Tool JSON must include tool_name")
        val schemaElement = root["tool_schema"] ?: root["toolSchema"]
        val toolSchema = when (schemaElement) {
            null, JsonNull -> null
            is JsonObject -> schemaElement.toString()
            is JsonPrimitive -> schemaElement.contentOrNull
                ?: throw IllegalArgumentException("tool_schema string must not be null")
            else -> throw IllegalArgumentException(
                "tool_schema must be a JSON object, string, or null"
            )
        }

        return ParsedToolApiJsonRequest(
            prompt = prompt,
            toolName = toolName,
            toolSchema = toolSchema?.takeIf { it.isNotBlank() },
        )
    }

    private fun requiredString(root: JsonObject, key: String): String {
        return optionalString(root, key)
            ?: throw IllegalArgumentException("Tool JSON must include $key")
    }

    private fun optionalString(root: JsonObject, key: String): String? {
        val value = root[key] as? JsonPrimitive ?: return null
        return value.contentOrNull?.trim()?.takeIf { it.isNotEmpty() }
    }
}
