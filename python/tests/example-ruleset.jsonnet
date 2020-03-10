[
    {
        rule: "(and (= direction 'extract') (or (= type 'depo') (= type 'frame')))",
        rw: "w",
        filepat: "{jobname}.hdf",
        grouppat: "{extra}/{stream}/{type}",
        attr: {extra:"foo"}
    },
    {
        rule: "(and (= direction 'inject') (or (= type 'depo') (= type 'frame')))",
        rw: "r",
        filepat: "{jobname}.hdf",
        grouppat: "{extra}/{stream}/{type}",
        attr: {extra:"bar"}
    },
]
