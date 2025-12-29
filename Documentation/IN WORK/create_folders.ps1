# need to run first -- Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process -Force
# Define the target directory path
$targetPath = "C:\Users\Fat Penelope\Desktop\code\TestCppUtilities\TestCppUtilities\Documentation"

# Define the list of folder names
$folderNames = "CheckedArithmetic",
				"CircularBuffer",
				"ConcurrencyPolicies",
				"ConstexprUtilities",
				"ContractException",
				"DebugOnly",
				"DiagnosticLogger",
				"Enforce",
				"EnumPlus",
				"Expected",
				"FatPJsonLite",
				"FatPTest",
				"FatPTypeTraits",
				"FeatureManager",
				"FlatMap_FlatSet",
				"FloatingPointComparison",
				"JsonLite",
				"PipeOperator",
				"ScopeGuard",
				"Signal",
				"Stringify",
				"StringPool",
				"StrongId",
				"TypeTraits"
				
# Loop through each name and create the directory
foreach ($name in $folderNames) {
    New-Item -Path $targetPath -Name $name -ItemType Directory
}

Write-Host "Script finished."
Read-Host -Prompt "Press Enter to exit"