Set-StrictMode -Version 3.0
$Lookup = Import-Csv ".\shader_struct_types.txt"
$LookuphashTable = @{}
$Lookup | Foreach-Object {$LookuphashTable[$_.hlsl] = $_.cpp}

function Replace-From-Table
{
    param([hashtable] $table, [string] $target)
    Foreach($k in $table.Keys)
    {
        $target = $target -replace "$k ", "$($LookuphashTable[$k]) "
    }
    return $target 
}

function generate-structs-hlsl
{

    $contents =  Get-content "./Shaders\structs.hlsl" -Raw 


    # Replace general file contents
    $contents = Replace-From-Table $LookuphashTable $contents

    # Write-Host $contents

    $structNameRegex = "(?<=struct)\s+(\S*)\s*(?={)"
    # Write-Host ".."
    $structnames = (Select-String -InputObject $contents -Pattern $structNameRegex -AllMatches).Matches

    Foreach($s in $structnames)
    {
        $sname = $s.ToString().trim()
        $contents = $contents -replace $("(?-i)$sname"), "GPU_$sname"
    }
    # Write-Host $contents

    $constFloatSemicolonRegex = "(?<=static constexpr float.*=.*\d*.*)(?<!\s*f\s*)(;)"
    $floatSeicolonMatches = (Select-String -InputObject $contents -Pattern $constFloatSemicolonRegex -AllMatches) |Select-Object -ExpandProperty Matches 
    Write-Host $($floatSeicolonMatches.length) 
    $indexOffset = 0 #As we add insertions first to last, we have to bump the index 
    $insertString = "f"
    Foreach($m in $floatSeicolonMatches)
    {
        Write-Host ($m.groups[0].index) -foregroundColor Green 
        $index = $m.groups[0].index
        $contents = $contents.Insert($index + $indexOffset,  $insertString)
        $indexOffset +=  $insertString.Length
    }

    $output = "//GENERATED FILE`n//EDIT structs.hlsl TO UPDATE`n#pragma once"+ "`n"+'#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>'+"`n"+ '#include <General/GLM_IMPL.h>'+ "`n"
    $output += $contents

    # Write-Host $output 
    $output | Out-File Code/Renderer/VulkanIncludes/structs_hlsl.h -Encoding ascii
}

# function process-binding-line
# {
#  param([string] $line)
#  $result = ""
#  $oneBindingRegex = "(?<=binding\()(\d+)\s*(?=\))"
#  $twoBindingRegex = "(?<=binding\()(\d+),\s*(\d+)"
#  if ($line -like "*push_constant*")
#  {
#     $result = "pushconstant"
#      return $result
#  }   
#  else 
#  {
    
#     $search = (Select-String -InputObject $line -Pattern $twoBindingRegex)
#     if ($search)
#     { 
#      $result = "$($search.Matches)"
#      return $result
#     }

#     $search = (Select-String -InputObject $line -Pattern $oneBindingRegex)
#     if ($search)
#     { 
#      $result = "$($search.Matches), 0"
#       return $result
#     }
#      Write-Host "search failed"
#  }

# }
# function generate-LayoutInfo
# {
#    $items = Get-ChildItem -Path "./Shaders/*" -Filter **.hlsl
#   $items |Foreach-Object{Write-Host $_}
#    $items |Foreach-Object{
#     $shaderfile = $_

#     $bindings = @()
#     $reading = $false
#     foreach ($line in Get-Content -Path   $shaderfile) {
#         if ($reading)
#         {
#             #ingest every char until we find ';' 
#             #Then split the line to get arg type and name
#         }
#         else 
#         {
#     if ($line -like '`[`[**`]`]')
#     { 
#       $reading = #true;
#      $bindings += process-binding-line($line)
#     }
#         }
#     }


#    }

# }
generate-structs-hlsl
# generate-LayoutInfo