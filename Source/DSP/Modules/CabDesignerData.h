/*
    Copyright 2025, 2026 iamlamprey

    This file is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This file is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with This file. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

namespace Altar::DSP
{

namespace CabDesignerData
{
    enum FilterType { HighPass, LowPass, Peak, HighShelf, LowShelf };

    struct FilterSpec
    {
        FilterType type { Peak };
        float freq { 1000.0f };
        float gainDB { 0.0f };
        float Q { 0.707f };
        bool enabled { true };
    };

    static constexpr FilterSpec FS(FilterType t, float f, float g, float q) { return { t, f, g, q, true }; }

    inline static const FilterSpec speakerA[] = {
        FS(HighPass,    93.0f,      0.0f,    0.6f),        
        FS(Peak,        220.0f,    -4.5f,    6.5f),
        FS(Peak,        805.0f,    -2.8f,    6.0f),
        FS(Peak,        1200.0f,   -8.2f,    8.0f),
        FS(Peak,        1500.0f,   -10.0f,   8.0f),
        FS(Peak,        2200.0f,    6.8f,    8.0f),
        FS(Peak,        2400.0f,    3.5f,    4.2f),        
        FS(LowPass,     6000.0f,    0.0f,    0.707f),            
    };

    inline static const FilterSpec speakerB[] = {        
        FS(HighPass,    100.0f,      0.0f,   0.6f),    
        FS(Peak,        350.0f,    -1.4f,    2.2f),
        FS(Peak,        700.0f,     0.9f,    1.0f),
        FS(Peak,        830.0f,    -1.6f,    3.7f),
        FS(Peak,        1400.0f,   -3.6f,    2.2f),
        FS(Peak,        2300.0f,    5.0f,    2.4f),
        FS(Peak,        3500.0f,    3.5f,    3.4f),        
        FS(LowPass,     6000.0f,    0.0f,    0.707f),        
    };

    inline static const FilterSpec speakerC[] = {        
        FS(HighPass,    97.0f,      0.0f,    0.6f),    
        FS(Peak,        404.0f,    -1.5f,    2.2f),        
        FS(Peak,        1400.0f,   -6.8f,    2.9f),
        FS(Peak,        2300.0f,    7.6f,    6.2f),
        FS(Peak,        3300.0f,    4.9f,    6.3f),
        FS(Peak,        5300.0f,    2.8f,    8.0f),
        FS(LowPass,     6000.0f,    0.0f,    0.707f),
    };

    inline static const FilterSpec speakerD[] = {
        FS(HighPass,    80.0f,      0.0f,    0.6f),        
        FS(Peak,        239.0f,    -1.1f,    4.4f),        
        FS(Peak,        342.0f,    -1.4f,    6.8f),    
        FS(Peak,        502.0f,     1.7f,    5.4f),    
        FS(Peak,        904.0f,    -1.0f,    8.0f),    
        FS(Peak,        1400.0f,   -3.6f,    4.2f),    
        FS(Peak,        2200.0f,    5.5f,    4.9f),    
        FS(Peak,        3000.0f,    4.4f,    5.2f),    
        FS(LowPass,     6000.0f,    0.0f,    0.707f),
    };

    inline static const FilterSpec speakerE[] = {
        FS(HighPass,    60.0f,      0.0f,    0.6f),
        FS(Peak,        153.0f,     1.5f,    0.4f),
        FS(Peak,        402.0f,    -1.9f,    0.9f),
        FS(Peak,        797.0f,     1.8f,    1.0f),
        FS(Peak,        1600.0f,   -3.8f,    2.2f),
        FS(Peak,        2300.0f,    5.5f,    4.0f),
        FS(Peak,        3300.0f,    4.2f,    3.9f),
        FS(LowPass,     6000.0f,    0.0f,    0.707f),
    };

    inline static const FilterSpec micA[] = {
        FS(HighPass,    130.0f,     0.0f,   0.707f),
        FS(HighPass,    130.0f,     0.0f,   0.707f),
        FS(Peak,        6000.0f,    6.0f,   1.0f),
        FS(LowPass,     14000.0f,   0.0f,   0.707f),
        FS(LowPass,     16000.0f,   0.0f,   0.707f),
    };

    inline static const FilterSpec micB[] = {
        FS(HighPass,    70.0f,      0.0f,   0.707f),
        FS(HighPass,    70.0f,      0.0f,   0.707f),
        FS(Peak,        4000.0f,    6.0f,   1.0f),
        FS(HighShelf,   10000.0f,   4.0f,   1.0f),
        FS(LowPass,     14000.0f,   0.0f,   0.707f),
        FS(LowPass,     16000.0f,   0.0f,   0.707f),
    };

    inline static const FilterSpec micC[] = {
        FS(HighPass,    80.0f,      0.0f,   0.707f),
        FS(HighPass,    80.0f,      0.0f,   0.707f),
        FS(Peak,        1200.0f,   -3.0f,   3.0f),
        FS(Peak,        6500.0f,    4.0f,   3.0f),
        FS(Peak,        13000.0f,   5.0f,   2.0f),
        FS(LowPass,     15000.0f,   0.0f,   0.707f),
        FS(LowPass,     16000.0f,   0.0f,   0.707f),
    };

    inline static const FilterSpec micD[] = {
        FS(HighPass,    60.0f,      0.0f,   0.707f),        
        FS(Peak,        8500.0f,    5.0f,   0.8f),        
        FS(LowPass,     16000.0f,   0.0f,   0.707f),
        FS(LowPass,     16000.0f,   0.0f,   0.707f),
    };

    inline static const FilterSpec micE[] = {
        FS(HighPass,    30.0f,      0.0f,   0.707f),    
        FS(LowShelf,    60.0f,      2.0f,   1.0f),    
        FS(Peak,        500.0f,    -2.0f,   0.5f),        
        FS(Peak,        1300.0f,    1.5f,   0.5f),   
        FS(HighShelf,   13500.0f,  -3.0f,   1.0f),
        FS(LowPass,     18000.0f,   0.0f,   0.707f),
        FS(LowPass,     18000.0f,   0.0f,   0.707f),
    };

    inline static const FilterSpec customMod[] = {
        FS(HighPass,    50.0f,      0.0f,   0.707f),
        FS(HighPass,    50.0f,      0.0f,   0.707f),        
        FS(Peak,        123.0f,     8.0f,   0.4f),  
        FS(Peak,        200.0f,     4.0f,   0.3f),  
        FS(Peak,        498.0f,    -4.4f,   4.9f),  
        FS(Peak,        2400.0f,   -3.6f,   2.9f),  
        FS(Peak,        3900.0f,    2.9f,   0.6f), 
        FS(Peak,        4500.0f,    7.7f,   0.7f),          
        FS(LowPass,     5500.0f,    0.0f,   0.7f),        
        FS(LowPass,     5500.0f,    0.0f,   0.7f),        
        FS(LowPass,     5500.0f,    0.0f,   0.7f),        
    };
}

} // namespace Altar::DSP
