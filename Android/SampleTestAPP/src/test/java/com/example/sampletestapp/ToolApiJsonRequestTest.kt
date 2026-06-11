package com.example.sampletestapp

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ToolApiJsonRequestTest {
    @Test
    fun parsesObjectSchemaToCompactJson() {
        val parsed = ToolApiJsonRequest.parse(
            """
            {
              "prompt": "Return a search request.",
              "tool_name": "web_search",
              "tool_schema": {
                "type": "object",
                "properties": {
                  "query": {"type": "string"},
                  "count": {"type": "integer"}
                },
                "required": ["query"]
              }
            }
            """.trimIndent()
        )

        assertEquals("Return a search request.", parsed.prompt)
        assertEquals("web_search", parsed.toolName)
        assertEquals(
            """{"type":"object","properties":{"query":{"type":"string"},"count":{"type":"integer"}},"required":["query"]}""",
            parsed.toolSchema
        )
    }

    @Test
    fun acceptsCamelCaseAndStringSchema() {
        val parsed = ToolApiJsonRequest.parse(
            """
            {
              "prompt": "Return an answer.",
              "toolName": "answer_tool",
              "toolSchema": "{\"type\":\"object\"}"
            }
            """.trimIndent()
        )

        assertEquals("Return an answer.", parsed.prompt)
        assertEquals("answer_tool", parsed.toolName)
        assertEquals("""{"type":"object"}""", parsed.toolSchema)
    }

    @Test
    fun allowsMissingSchemaForPreloadedToolsetEntries() {
        val parsed = ToolApiJsonRequest.parse(
            """
            {
              "prompt": "Use the preloaded schema.",
              "tool_name": "preloaded_tool"
            }
            """.trimIndent()
        )

        assertEquals("Use the preloaded schema.", parsed.prompt)
        assertEquals("preloaded_tool", parsed.toolName)
        assertNull(parsed.toolSchema)
    }
}
