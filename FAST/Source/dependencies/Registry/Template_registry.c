char *template_registry[] = {
"##################################################################################################################################",
"# Registry for ModuleName, creates MODULE ModuleName_Types",
"# Module ModuleName_Types contains all of the user-defined types needed in ModuleName. It also contains copy, destroy, pack, and",
"# unpack routines associated with each defined data types.",
"##################################################################################################################################",
"# Entries are of the form",
"# keyword <modulename/modname> <TypeBeingDefined> <FieldType> <FieldName> <Dims> <InitialValue> <Ctrl> \"<DESCRIP>\" \"<UNITS>\"",
"##################################################################################################################################",
"",
"# ..... Initialization data .......................................................................................................",
"# Define inputs that the initialization routine may need here:",
"# e.g., the name of the input file, the file root name, etc.",
"typedef ModuleName/ModName InitInputType CHARACTER(1024) InputFile - - - \"Name of the input file; remove if there is no file\" -",
"# Define outputs that the initialization routine may need here:",
"# e.g., the name of the input file, the file root name, etc.",
"typedef ModuleName/ModName InitOutputType Reki DummyInitVar - - - \"A variable\" -",
"",
"# ..... States ....................................................................................................................",
"# Define continuous (differentiable) states here:",
"typedef ModuleName/ModName ContinuousStateType ReKi DummyContState - - - \"A variable, Replace if you have continuous states\" -",
"",
"# Define discrete (nondifferentiable) states here:",
"typedef ModuleName/ModName ModName_DiscreteStateType ReKi DummyDiscState - - - \"A variable, Replace if you have discrete states\" -",
"",
"# Define constraint states here:",
"typedef ModuleName/ModName ConstraintStateType ReKi DummyConstrState - - - \"A variable, Replace if you have constraint states\" -",
"",
"# Define any data that are not considered actual states here:",
"# e.g. data used only for efficiency purposes (indices for searching in an array, copies of previous calculations of output",
"# at a given time, etc.)",
"typedef ModuleName/ModName OtherStateType IntKi DummyOtherState - - - \"A variable, Replace if you have other states\" -",
"",
"# ..... Parameters ................................................................................................................",
"# Define parameters here:",
"# Time step for integration of continuous states (if a fixed-step integrator is used) and update of discrete states:",
"typedef ModuleName/ModName ParameterType DbKi DT - - - \"Time step for cont. state integration & disc. state update\" seconds",
"",
"# ..... Inputs ....................................................................................................................",
"# Define inputs that are contained on the mesh here:",
"#typedef ModuleName ModName_InputType MeshType MeshedInput - - - \"Meshed data\" -",
"# Define inputs that are not on this mesh here:",
"typedef ModuleName/ModName InputType ReKi DummyInput - - - \"A variable, replace if you have input data\" -",
"",
"# ..... Outputs ...................................................................................................................",
"",
"# Define outputs that are not on this mesh here:",
"typedef ModuleName/ModName OutputType ReKi DummyOutput - - - \"A variable, replace if you have output data\" -",
0L } ;