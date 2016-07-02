# phalanx

Requirements:

- Combine GDS data of cells into larger macro and output GDS.
- Calculate tpd, tcd, tsu, tho of pins of larger macro, output as LIB. Timing data of smaller cells:
    - delay vs. load capacitance
    - delay vs. load capacitance vs. input slew
    - output slew vs. load capacitance
    - output slew vs. load capacitance vs. input slew
- Calculate leakage power and internal energy of larger macro, output as LIB.
- Derive LEF file from larger macro and output.
