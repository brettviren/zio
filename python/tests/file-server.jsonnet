[
    {
        rule: "(= direction 'inject')",
        rw: "w",
        filepat: "file-server-test.hdf",
        grouppat: "test/{stream}",
    },
    {
        rule: "(= direction 'extract')",
        rw: "r",
        filepat: "file-server-test.hdf",
        grouppat: "test/{stream}",
    }
]
