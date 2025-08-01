# Inference Engine Examples

This directory contains examples demonstrating various features of the kolosal-server inference engine.

## JSON Schema Mode

The `json_schema_example.cpp` file demonstrates how to use JSON Schema to constrain model outputs to specific structures. This is a higher-level alternative to writing GBNF grammars directly.

### What is JSON Schema Mode?

JSON Schema mode allows you to define the structure of expected JSON output using standard JSON Schema notation. The inference engine automatically converts your schema to the appropriate GBNF grammar, ensuring the model only generates valid JSON that conforms to your schema.

### Benefits of JSON Schema Mode

- **Easier to write**: JSON Schema is more intuitive than GBNF grammar
- **Industry standard**: Widely used for API validation and documentation
- **Rich validation**: Supports complex constraints like min/max values, string patterns, enums
- **Type safety**: Ensures generated data matches expected types
- **Reusable**: Schemas can be shared between frontend validation and LLM generation

### Example JSON Schema

```json
{
  "type": "object",
  "properties": {
    "name": {
      "type": "string",
      "description": "Person's full name"
    },
    "age": {
      "type": "integer",
      "minimum": 0,
      "maximum": 150
    },
    "email": {
      "type": "string",
      "format": "email"
    }
  },
  "required": ["name", "age", "email"]
}
```

### Using JSON Schema in Code

```cpp
// Define your schema
nlohmann::json schema = {
    {"type", "object"},
    {"properties", {
        {"name", {"type", "string"}},
        {"age", {"type", "integer"}},
        {"active", {"type", "boolean"}}
    }},
    {"required", {"name", "age"}}
};

// Use with text completion
CompletionParameters params;
params.prompt = "Generate user data:";
params.jsonSchema = schema.dump();

// Use with chat completion
ChatCompletionParameters chatParams;
chatParams.jsonSchema = schema.dump();
```

### Supported JSON Schema Features

- **Basic types**: string, number, integer, boolean, null, array, object
- **String constraints**: minLength, maxLength, pattern, format (email, date, etc.)
- **Number constraints**: minimum, maximum, exclusiveMinimum, exclusiveMaximum
- **Array constraints**: items, minItems, maxItems, uniqueItems
- **Object constraints**: properties, required, additionalProperties
- **Enums**: enum keyword for restricting to specific values
- **Nested objects and arrays**: Full support for complex nested structures

### JSON Schema vs Grammar

Choose JSON Schema when:
- You need to generate structured JSON data
- You want to leverage existing JSON Schema definitions
- You prefer a declarative approach to defining structure
- You need complex validation rules

Choose GBNF Grammar when:
- You need to generate non-JSON formats (code, markdown, etc.)
- You need fine-grained control over the exact output format
- You're generating domain-specific languages
- Performance is critical (direct grammar may be slightly faster)

## Grammar-Constrained Generation

The `grammar_example.cpp` file demonstrates how to use GBNF (Grammar-Based Natural Format) grammars to constrain the output of language models.

### What is GBNF?

GBNF is a grammar format similar to BNF (Backus-Naur Form) that allows you to define strict rules for generated text. This is particularly useful when you need the model to output:
- Structured data (JSON, XML, YAML)
- Code in specific programming languages
- Formatted lists or tables
- Restricted responses (Yes/No, multiple choice)

### Basic Grammar Syntax

- `::=` defines a rule
- `|` represents alternatives (OR)
- `()` groups elements
- `?` makes an element optional
- `*` allows zero or more repetitions
- `+` requires one or more repetitions
- `[]` defines character classes
- `""` defines literal strings

### Example Grammars

#### JSON Grammar
```gbnf
root   ::= object
value  ::= object | array | string | number | ("true" | "false" | "null") ws
object ::= "{" ws (string ":" ws value ("," ws string ":" ws value)*)? "}" ws
array  ::= "[" ws (value ("," ws value)*)? "]" ws
string ::= "\"" ([^"\\] | "\\" (["\\/bfnrt] | "u" [0-9a-fA-F]{4}))* "\"" ws
number ::= ("-"? ([0-9] | [1-9] [0-9]*)) ("." [0-9]+)? ([eE] [-+]? [0-9]+)? ws
ws     ::= ([ \t\n] ws)?
```

#### Simple Yes/No Grammar
```gbnf
root ::= ("Yes" | "No")
```

#### Email Address Grammar
```gbnf
root ::= local "@" domain
local ::= [a-zA-Z0-9._%+-]+
domain ::= [a-zA-Z0-9.-]+ "." [a-zA-Z]{2,}
```

### Using Grammars in Code

```cpp
// For text completion
CompletionParameters params;
params.prompt = "Generate a JSON object:";
params.grammar = json_grammar_string;
params.maxNewTokens = 200;

// For chat completion
ChatCompletionParameters chatParams;
chatParams.messages.push_back(Message("user", "Describe a person"));
chatParams.grammar = json_grammar_string;
```

### Best Practices

1. **Test Your Grammar**: Always test your grammar with simple examples first
2. **Temperature Settings**: Use lower temperatures (0.1-0.5) for more deterministic outputs when using strict grammars
3. **Token Limits**: Set appropriate `maxNewTokens` based on expected output length
4. **Error Handling**: Always check for errors as grammar constraints might cause generation to fail
5. **Whitespace**: Include whitespace rules (`ws`) in your grammar for better formatting

### Common Use Cases

- **API Response Formatting**: Ensure LLM outputs valid JSON for API responses
- **Code Generation**: Generate syntactically correct code snippets
- **Data Extraction**: Extract structured data from unstructured text
- **Form Filling**: Generate responses that fit specific form fields
- **Decision Making**: Constrain outputs to specific choices

### Troubleshooting

- If generation stops unexpectedly, the grammar might be too restrictive
- If you get parsing errors, check your grammar syntax
- Use simpler grammars first and gradually add complexity
- Enable debug logging to see how tokens are being constrained

## Building and Running Examples

```bash
# Build the example
cd /path/to/kolosal-server/inference
mkdir build && cd build
cmake ..
make

# Run the grammar example
./examples/grammar_example
```

Make sure to update the model path in the example code before running.