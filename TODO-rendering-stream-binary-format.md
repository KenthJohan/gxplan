# TODO: Define rendering stream binary format

- **Priority:** Low
- **Status:** Deferred

The portable rendering opcode binary format is intentionally deferred while the
C rendering API is being explored. Commands are authored and pushed from C;
a standalone interpreter or external wire format is not a current requirement.
The current byte-stream prototype is not a settled serialization contract.

## Current API work

- Design commands as higher-level rendering operations. Allow variable-length
  data, such as strings, arrays, nested descriptions and uploads, without
  constraining the API to a particular binary encoding.
- Define each command's fields, validation and error behavior.
- Decide whether execution is immediate or deferred and document data
  ownership and lifetime, including any copies required for deferred work.
- Keep any in-memory command representation consistent with these decisions;
  it does not need to be portable or stable across versions.

## When a portable format is needed

- Choose an encoding that represents the C API's variable-length data and
  document record boundaries, payloads, alignment, byte order and layout.
- Define protocol versioning, compatibility, validation and error handling.
- Align the implementation and tests with the chosen contract.
