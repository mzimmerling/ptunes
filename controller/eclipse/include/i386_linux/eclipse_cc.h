typedef void *generic_ptr;
typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef int word;
typedef unsigned int uword;
typedef long *void_ptr;
typedef int a_mutex_t;
struct dict_item
{
    long arity;
    struct s_pword *string;
};
typedef struct dict_item *dident;
typedef void_ptr (*func_ptr)(void);
typedef func_ptr (*continuation_t)(void);
typedef union
{
 uword all;
 uword *wptr;
 struct s_pword *ptr;
 char *str;
 word nint;
 dident did;
} value;
typedef union
{
 uword all;
 word kernel;
} type;
typedef struct s_pword
{
 value val;
 type tag;
} pword;
typedef uword vmcode;
typedef uint32 aport_handle_t;
typedef aport_handle_t site_handle_t;
typedef struct st_handle_ds {
 site_handle_t site;
 void_ptr edge;
 void_ptr knot;
} st_handle_t;
typedef struct stack_struct {
 char *name;
 uword *start,
  *end,
  *peak;
} stack_pair[2];
enum ec_ref_state { EC_REF_FREE=0,EC_REF_C=1,EC_REF_P=2,EC_REF_C_P=3 };
typedef struct eclipse_ref_
{
 pword var;
 struct eclipse_ref_ * prev;
 struct eclipse_ref_ * next;
 enum ec_ref_state refstate;
 int size;
} * ec_ref;
typedef ec_ref ec_refs;
typedef void_ptr stream_id;
typedef struct {
 dident file;
 uword line;
 uword from;
 uword to;
} source_pos_t;
typedef struct
{
    pword debug_top;
    void_ptr fail_trace;
    word next_invoc;
    void_ptr call_proc;
    word call_port;
    word call_invoc;
    word first_delay_invoc;
    word redo_level;
    word fail_drop;
    word fail_culprit;
    word port_filter;
    word min_invoc;
    word max_invoc;
    word min_level;
    word max_level;
    word trace_mode;
    source_pos_t source_pos;
} trace_t;
typedef struct _dyn_event_q_slot_t {
    pword event_data;
    struct _dyn_event_q_slot_t *next;
} dyn_event_q_slot_t;
typedef struct {
    dyn_event_q_slot_t *prehead;
    dyn_event_q_slot_t *tail;
    unsigned long total_event_slots;
    unsigned long free_event_slots;
} dyn_event_q_t;
struct machine
{
    pword * sp;
    pword ** tt;
    pword * tg;
    pword * e;
    pword * eb;
    pword * gb;
    pword *
  b;
    pword * lca;
    int vm_flags;
    volatile int event_flags;
    vmcode *pp;
    pword * de;
    pword * ld;
    pword * mu;
    pword * sv;
    pword wl;
    int wp;
    pword wp_stamp;
    pword postponed_list;
    pword * pb;
    pword * ppb;
    st_handle_t *leaf;
    int load;
    pword * occur_check_boundary;
    pword * top_constructed_structure;
    pword * oracle;
    char * followed_oracle;
    char * pending_oracle;
    int ntry;
    int global_bip_error;
    trace_t trace_data;
    pword * gctg;
    pword * volatile tg_soft_lim;
    pword * tg_soft_lim_shadow;
    volatile int irq_faked_overflow;
    pword * tg_limit;
    pword ** tt_limit;
    pword * b_limit;
    pword * sp_limit;
    stack_pair global_trail;
    stack_pair control_local;
    long segment_size;
    int nesting_level;
    void_ptr parser_env;
    void_ptr
  it_buf;
    pword posted;
    pword posted_last;
    struct eclipse_ref_ allrefs;
    pword *global_variable;
    pword emu_args[(1 + 255 + 8 + 2 )];
    dyn_event_q_t dyn_event_q;
};
struct tag_descriptor {
 type tag;
 long super;
 dident tag_name;
 dident type_name;
 int numeric;
 int order;
 int (* write)(int,stream_id,value,type);
 int (* string_size)(value,type,int);
 int (* to_string)(value,type,char*,int);
 int (* from_string)(char *,pword*,int);
 int (* equal)(pword*,pword*);
 int (* compare)(value,value);
 int (* arith_compare)(value,value,int*);
 int (* copy_size)(value,type);
 pword * (* copy_to_heap)(value,type,pword*,pword*);
 pword * (* copy_to_stack)(void);
 int (* arith_op[53])(...);
 int (* coerce_to[13 +1])(value,value*);
};
typedef void *t_ext_ptr;
typedef struct {
    void (*free)(t_ext_ptr);
    t_ext_ptr (*copy)(t_ext_ptr);
    void (*mark_dids)(t_ext_ptr);
    int (*string_size)(t_ext_ptr obj, int quoted_or_radix);
    int (*to_string)(t_ext_ptr obj, char *buf, int quoted_or_radix);
    int (*equal)(t_ext_ptr, t_ext_ptr);
    t_ext_ptr (*remote_copy)(t_ext_ptr);
    pword (*get)(t_ext_ptr, int);
    int (*set)(t_ext_ptr, int, pword);
} t_ext_type;
typedef struct {
    pword goal;
    pword module;
    word ref_ctr;
    short enabled;
    short defers;
} t_heap_event;
struct shared_data_t {
 a_mutex_t
  general_lock,
  mod_desc_lock,
  prop_desc_lock,
      prop_list_lock,
  proc_desc_lock,
      proc_list_lock,
      proc_chain_lock,
      assert_retract_lock;
 int global_flags,
  print_depth,
  output_mode_mask,
  compile_id,
  code_heap_used,
  global_var_index,
  load_release_delay,
  publishing_param,
  nbstreams,
  user_error,
  max_errors,
  symbol_table_version,
  dyn_global_clock,
  dyn_killed_code_size,
  dyn_num_of_kills;
 void_ptr
  dictionary,
  abolished_dynamic_procedures,
  abolished_procedures,
  compiled_structures,
  dynamic_procedures,
  global_procedures,
  constant_table,
  stream_descriptors,
  error_handler,
  default_error_handler,
  interrupt_handler,
  interrupt_handler_flags,
  interrupt_name,
  error_message,
  message,
  startup_goal,
  debug_macros,
  worker_statistics,
  extension_ptr,
  extension_ptr1,
  extension_ptr2,
  extension_ptr3,
  extension_ptr4,
  extension_ptr5,
  extension_ptr6,
  extension_ptr7;
};
enum t_allocation { ALLOC_PRE,ALLOC_FIXED,ALLOC_VIRTUAL } ;
enum t_io_option { SHARED_IO,OWN_IO,MEMORY_IO } ;
typedef struct
{
    int option_p;
    char *mapfile;
    int parallel_worker;
    int io_option;
    char **Argv;
    int Argc;
    int rl;
    uword localsize;
    uword globalsize;
    uword privatesize;
    uword sharedsize;
    void (*user_panic)(const char*,const char *);
    int allocation;
    char *default_module;
    char *eclipse_home;
    int init_flags;
    int debug_level;
} t_eclipse_options;
typedef struct
{
    dident
 abort,
 apply2,
 at2,
 block,
 block_atomic,
 call_body,
 comma,
 cond,
 cut,
 emulate,
 exit_block,
 fail,
 kernel_sepia,
 list,
 nil,
 semicolon,
 colon,
 sepia,
 true0,
 abolish,
 abs,
 acos,
 all,
 ampersand,
 and2,
 answer,
 append,
 arg,
 subscript,
 asin,
 atan,
 atom,
 atom0,
 atomic,
 bar,
 bitnot,
 bignum,
 break0,
 bsi,
 built_in,
 local_control_overflow,
 byte,
 call,
 call_explicit,
 clause,
 clause0,
 cn,
 command,
 comment,
 compile,
 compile_stream,
 compound,
 compound0,
 constrained,
 cos,
 cprolog,
 cut_to,
 debug,
 log_output,
 warning_output,
 debugger,
 default0,
 default_module,
 define_global_macro3,
 define_local_macro3,
 delay,
 denominator1,
 diff_reg,
 div,
 dummy_call,
 dynamic,
 e,
 eclipse_home,
 ellipsis,
 empty,
 eocl,
 eof,
 eoi,
 equal,
 erase_macro1,
 err,
 eerrno,
 error,
 error_handler,
 exit_postponed,
 exp,
 export1,
 exportb,
 external,
 fail_if,
 fail_if0,
 file_query,
 breal,
 breal1,
 breal_from_bounds,
 breal_min,
 breal_max,
 fix,
 float0,
 float1,
 floor1,
 flush,
 free,
 free1,
 from,
 functor,
 garbage_collect_dictionary,
 global,
 global0,
 globalb,
 go,
 goal,
 goalch,
 grammar,
 greater,
 greaterq,
 ground,
 handle_expired_while_thrown,
 halt,
 halt0,
        hang,
 identical,
 if2,
 import,
 import_fromb,
 inf,
 inf0,
 infq,
 infq0,
 input,
 integer,
 integer0,
 invoc,
 is_event,
 is_handle,
 is_list,
 is_suspension,
 double1,
 double0,
 is,
 global_trail_overflow,
 leash,
 less,
 lessq,
 ln,
 local,
 local_break,
 local0,
 localb,
 lock,
 lshift,
 macro,
 make_suspension,
 max,
 maxint,
 meta,
 meta0,
 metacall,
 min,
 minint,
 minus,
 minus0,
 minus1,
 mode,
 module0,
 module1,
 module_directive,
 modulo,
 nilcurbr,
 nilcurbr1,
 no_err_handler,
 nodebug,
        nohang,
 nonground,
 nonvar,
 naf,
 not0,
 not1,
 not_equal,
 not_identical,
 not_not,
 not_unify,
 notp0,
 notrace,
 null,
 number,
 numerator1,
 off,
 on,
 once,
 once0,
 or2,
 output,
 pcompile,
 pi,
 plus,
 plus0,
 plus1,
 plusplus,
 power,
 pragma,
 print,
 priority,
 prolog,
 protect_arg,
 question,
 quintus,
 quotient,
 rational0,
 rational1,
 read,
 read1,
 read2,
 real,
 real0,
 reset,
 round,
 rshift,
 rulech0,
 rulech1,
 rulech2,
 semi0,
 sicstus,
 sin,
 skip,
 softcut,
 some,
 spy,
 sqrt,
 state,
 stderr0,
 stdin0,
 stdout0,
 stop,
 string0,
 string,
 sup,
 sup0,
 supq,
 supq0,
 suspending,
 suspend_attr,
 syscut,
 syserror,
 system,
 system_debug,
 tan,
 term,
 times,
 top_only,
 trace,
 trace_frame,
 trans_term,
 unify,
 unify0,
 univ,
 universally_quantified,
 update,
 uref,
 user,
 var,
 var0,
 wake,
 with2,
 with_attributes2,
 write,
 write1,
 write2,
 writeq1,
 writeq2,
 xor2;
} standard_dids;
typedef struct
{
    struct machine m;
    struct shared_data_t *shared;
    struct tag_descriptor td[13 +1];
    standard_dids d;
} t_eclipse_data;
enum {
 EC_OPTION_MAPFILE =0,
 EC_OPTION_PARALLEL_WORKER =1,
 EC_OPTION_ARGC =2,
 EC_OPTION_ARGV =3,
 EC_OPTION_LOCALSIZE =4,
 EC_OPTION_GLOBALSIZE =5,
 EC_OPTION_PRIVATESIZE =6,
 EC_OPTION_SHAREDSIZE =7,
 EC_OPTION_PANIC =8,
 EC_OPTION_ALLOCATION =9,
 EC_OPTION_DEFAULT_MODULE =10,
 EC_OPTION_ECLIPSEDIR =11,
 EC_OPTION_IO =12,
 EC_OPTION_INIT =13,
 EC_OPTION_DEBUG_LEVEL =14
};
extern "C" t_eclipse_data ec_;
extern "C" t_ext_type ec_xt_double_arr;
extern "C" t_ext_type ec_xt_long_arr;
extern "C" t_ext_type ec_xt_char_arr;
extern "C" int ec_set_option_int (int, int);
extern "C" int ec_set_option_long (int, long);
extern "C" int ec_set_option_ptr (int, void *);
extern "C" int ec_init (void);
extern "C" int ec_cleanup (void);
extern "C" void ec_post_goal (const pword);
extern "C" void ec_post_string (const char *);
extern "C" void ec_post_exdr (int, const char *);
extern "C" int ec_resume (void);
extern "C" int ec_resume1 (ec_ref);
extern "C" int ec_resume2 (const pword,ec_ref);
extern "C" int ec_resume_long (long *);
extern "C" int ec_resume_async (void);
extern "C" int ec_running (void);
extern "C" int ec_resume_status (void);
extern "C" int ec_resume_status_long (long *);
extern "C" int ec_wait_resume_status_long (long *, int);
extern "C" int ec_post_event (pword);
extern "C" int ec_post_event_string (const char *);
extern "C" int ec_post_event_int (int);
extern "C" int ec_handle_events (long *);
extern "C" void ec_cut_to_chp (ec_ref);
extern "C" pword ec_string (const char*);
extern "C" pword ec_length_string (int, const char*);
extern "C" pword ec_atom (const dident);
extern "C" pword ec_long (const long);
extern "C" pword ec_double (const double);
extern "C" pword ec_term (dident, ... );
extern "C" pword ec_term_array (const dident,const pword[]);
extern "C" pword ec_list (const pword,const pword);
extern "C" pword ec_listofrefs (ec_refs);
extern "C" pword ec_listofdouble (int, const double*);
extern "C" pword ec_listoflong (int, const long*);
extern "C" pword ec_listofchar (int, const char*);
extern "C" pword ec_arrayofdouble (int, const double*);
extern "C" pword ec_matrixofdouble (int, int, const double*);
extern "C" pword ec_handle (const t_ext_type*,const t_ext_ptr);
extern "C" pword ec_newvar (void);
extern "C" pword ec_nil (void);
extern "C" dident ec_did (const char *,const int);
extern "C" int ec_get_string (const pword,char**);
extern "C" int ec_get_string_length (const pword,char**,long*);
extern "C" int ec_get_atom (const pword,dident*);
extern "C" int ec_get_long (const pword,long*);
extern "C" int ec_get_double (const pword,double*);
extern "C" int ec_get_nil (const pword);
extern "C" int ec_get_list (const pword,pword*,pword*);
extern "C" int ec_get_functor (const pword,dident*);
extern "C" int ec_arity (const pword);
extern "C" int ec_get_arg (const int,pword,pword*);
extern "C" int ec_get_handle (const pword,const t_ext_type*,t_ext_ptr*);
extern "C" int ec_is_var (const pword);
extern "C" ec_refs ec_refs_create (int,const pword);
extern "C" ec_refs ec_refs_create_newvars (int);
extern "C" void ec_refs_destroy (ec_refs);
extern "C" void ec_refs_set (ec_refs,int,const pword);
extern "C" pword ec_refs_get (const ec_refs,int);
extern "C" int ec_refs_size (const ec_refs);
extern "C" ec_ref ec_ref_create (pword);
extern "C" ec_ref ec_ref_create_newvar (void);
extern "C" void ec_ref_destroy (ec_ref);
extern "C" void ec_ref_set (ec_ref,const pword);
extern "C" pword ec_ref_get (const ec_ref);
extern "C" int ec_exec_string (char*,ec_ref);
extern "C" int ec_var_lookup (ec_ref,char*,pword*);
extern "C" pword ec_arg (int);
extern "C" int ec_unify (pword,pword);
extern "C" int ec_unify_arg (int,pword);
extern "C" int ec_compare (pword,pword);
extern "C" int ec_visible_procedure (dident,pword,void**);
extern "C" int ec_make_suspension (pword,int,void*,pword*);
extern "C" int ec_schedule_suspensions (pword,int);
extern "C" int ec_free_handle (const pword, const t_ext_type*);
extern "C" int ec_stream_nr (char *name);
extern "C" int ec_queue_write (int stream, char *data, int size);
extern "C" int ec_queue_read (int stream, char *data, int size);
extern "C" int ec_queue_avail (int stream);
extern "C" void ec_double_xdr (double * d, char * dest);
extern "C" void ec_int32_xdr (int32 * l, char * dest);
extern "C" void ec_xdr_int32 (char * buf , int32 * l);
extern "C" void ec_xdr_double (char * buf , double * d);
extern "C" char * ec_error_string (int);
extern "C" void ec_panic (const char* what, const char* where);
enum EC_status
{
 EC_succeed = 0,
 EC_fail = 1,
 EC_throw = 2,
 EC_yield = 4,
 EC_running = 5,
 EC_waitio = 6,
 EC_flushio = 7
};
class EC_atom;
class EC_functor;
class EC_word;
class EC_ref;
class EC_refs;
class EC_atom
{
    public:
 dident d;
 EC_atom() {}
 EC_atom(char * s) { d = ec_did(s,0); }
 EC_atom(dident did)
 {
     if ((((dident)(did))->arity))
  ec_panic("atom arity != 0", "EC_atom::EC_atom(dident d)");
     d = did;
 }
 char * Name() { return ((char *)(((dident)(d))->string + 1)); }
 char * name() { return ((char *)(((dident)(d))->string + 1)); }
};
class EC_functor
{
    public:
 dident d;
 EC_functor() {}
 EC_functor(char * s,int arity)
 {
     if (!arity)
  ec_panic("functor arity == 0", "EC_functor::EC_functor(char*,int)");
     d = ec_did(s, arity);
 }
 EC_functor(dident did)
 {
     if (!(((dident)(did))->arity))
  ec_panic("functor arity == 0", "EC_functor::EC_functor(dident d)");
     d = did;
 }
 char * Name() { return ((char *)(((dident)(d))->string + 1)); }
 char * name() { return ((char *)(((dident)(d))->string + 1)); }
 int Arity() { return (((dident)(d))->arity); }
 int arity() { return (((dident)(d))->arity); }
};
class EC_word
{
 friend class EC_ref;
 friend class EC_refs;
 pword w;
    public:
     EC_word(const pword& pw)
 {
     w = pw;
 }
     EC_word()
 {
 }
     EC_word&
 operator=(const EC_word& ew)
 {
     w = ew.w;
     return *this;
 }
     EC_word(const char *s)
 {
     w = ec_string(s);
 }
     EC_word(const int l, const char *s)
 {
     w = ec_length_string(l, s);
 }
     EC_word(const EC_atom did)
 {
     w = ec_atom(did.d);
 }
     EC_word(const long l)
 {
     w = ec_long(l);
 }
     EC_word(const int i)
 {
     w = ec_long((long)i);
 }
     EC_word(const double d)
 {
     w = ec_double(d);
 }
     EC_word(const EC_ref& ref);
     friend EC_word
 term(const EC_functor functor,const EC_word args[]);
 friend EC_word
 term(const EC_functor functor, const EC_word arg1,
     const EC_word arg2,
     const EC_word arg3,
     const EC_word arg4);
 friend EC_word
 term(const EC_functor functor, const EC_word arg1,
     const EC_word arg2,
     const EC_word arg3,
     const EC_word arg4,
     const EC_word arg5,
     const EC_word arg6,
     const EC_word arg7,
     const EC_word arg8,
     const EC_word arg9,
     const EC_word arg10);
 friend EC_word
 list(const EC_word hd, const EC_word tl);
 int
 is_atom(EC_atom* did)
 {
  return ec_get_atom(w,(dident*) did);
 }
 int
 is_string(char **s)
 {
  return ec_get_string(w,s);
 }
 int
 is_string(char **s, long *len)
 {
  return ec_get_string_length(w,s,len);
 }
 int
 is_long(long * l)
 {
  return ec_get_long(w,l);
 }
 int
 is_double(double * d)
 {
  return ec_get_double(w,d);
 }
 int
 is_handle(const t_ext_type *cl, t_ext_ptr *data)
 {
  return ec_get_handle(w,cl,data);
 }
 int
 free_handle(const t_ext_type *cl)
 {
  return ec_free_handle(w,cl);
 }
 int
 is_list(EC_word& hd, EC_word& tl)
 {
  return ec_get_list(w, &hd.w, &tl.w);
 }
 int
 is_nil()
 {
  return ec_get_nil(w);
 }
 int
 is_var()
 {
  return ec_is_var(w);
 }
 int
 arity()
 {
  return ec_arity(w);
 }
 int
 functor(EC_functor* did)
 {
  return ec_get_functor(w, (dident*) did);
 }
 int
 arg(const int n,EC_word& arg)
 {
  return ec_get_arg(n, w, &arg.w);
 }
 friend int
 compare(const EC_word& term1, const EC_word& term2);
 friend int
 operator==(const EC_word& term1, const EC_word& term2);
 friend int
 unify(EC_word term1, EC_word term2);
 int
 unify(EC_word term)
 {
     return ec_unify(w, term.w);
 }
 int
 schedule_suspensions(int n)
 {
     return ec_schedule_suspensions(w, n);
 }
 friend void
 post_goal(const EC_word term);
 friend int
 EC_resume(EC_word term, EC_ref& chp);
 friend int
 EC_resume(EC_word term);
 friend int
 post_event(EC_word term);
};
inline int
compare(const EC_word& term1, const EC_word& term2)
{
    return ec_compare(term1.w, term2.w);
}
inline int
operator==(const EC_word& term1, const EC_word& term2)
{
    return ec_compare(term1.w, term2.w) == 0;
}
inline int
unify(EC_word term1, EC_word term2)
{
    return ec_unify(term1.w, term2.w);
}
inline void
post_goal(const EC_word term)
{
 ec_post_goal(term.w);
}
inline int
EC_resume(EC_word term)
{
    return ec_resume2(term.w,0);
}
inline int
post_event(EC_word term)
{
    return ec_post_event(term.w);
}
inline void
post_goal(const char * s)
{
 ec_post_string(s);
}
class EC_refs
{
    protected:
 ec_refs r;
    public:
     EC_refs(int size)
 {
  r = ec_refs_create_newvars(size);
 }
     EC_refs(int size,EC_word init)
 {
  r = ec_refs_create(size,init.w);
 }
     ~EC_refs()
 {
  ec_refs_destroy(r);
 }
 int size()
 {
  return ec_refs_size(r);
 }
 EC_word
 operator[](int index)
 {
  return EC_word(ec_refs_get(r,index));
 }
 friend EC_word
 list(EC_refs& array);
 void set(int index, EC_word new_value)
 {
     ec_refs_set(r, index, new_value.w);
 }
};
inline EC_word
list(EC_refs& array)
{
 return EC_word(ec_listofrefs(array.r));
}
class EC_ref
{
 friend class EC_word;
    protected:
 ec_refs r;
    public:
     EC_ref()
 {
  r = ec_refs_create_newvars(1);
 }
     EC_ref(EC_word init)
 {
  r = ec_refs_create(1,init.w);
 }
     ~EC_ref()
 {
  ec_refs_destroy(r);
 }
     EC_ref& operator=(const EC_word word);
 void cut_to()
 {
     ec_cut_to_chp(r);
 }
 friend int
 EC_resume(EC_ref& chp);
 friend int
 EC_resume(EC_word term, EC_ref& chp);
};
inline int
EC_resume()
{
    return ec_resume1(0);
}
inline int
EC_resume(EC_ref& chp)
{
    return ec_resume1(chp.r);
}
inline int
EC_resume(EC_word term, EC_ref& chp)
{
    return ec_resume2(term.w,chp.r);
}
inline EC_word
EC_arg(int n)
{
    return EC_word(ec_arg(n));
}
inline EC_word::EC_word(const EC_ref& ref)
{
 w = ec_refs_get(ref.r,0);
}
inline EC_ref&
EC_ref::operator=(const EC_word word)
{
 ec_refs_set(r,0,word.w);
 return *this;
}
inline EC_word
term(const EC_functor functor,const EC_word args[])
{
    EC_word t(ec_term_array(functor.d,(pword *) args));
    return t;
}
inline EC_word
term(const EC_functor functor, const EC_word arg1,
    const EC_word arg2 = 0,
    const EC_word arg3 = 0,
    const EC_word arg4 = 0)
{
    EC_word the_term(ec_term(functor.d,arg1.w,arg2.w,arg3.w,arg4.w));
    return the_term;
}
inline EC_word
term(const EC_functor functor, const EC_word arg1,
    const EC_word arg2,
    const EC_word arg3,
    const EC_word arg4,
    const EC_word arg5,
    const EC_word arg6 = 0,
    const EC_word arg7 = 0,
    const EC_word arg8 = 0,
    const EC_word arg9 = 0,
    const EC_word arg10 = 0)
{
    EC_word the_term(ec_term(functor.d,arg1.w,arg2.w,arg3.w,arg4.w,
    arg5.w,arg6.w,arg7.w,arg8.w,arg9.w,arg10.w));
    return the_term;
}
inline EC_word
list(const EC_word hd, const EC_word tl)
{
    EC_word t(ec_list(hd.w,tl.w));
    return t;
}
inline EC_word
list(int size, double* array)
{
 return EC_word(ec_listofdouble(size, array));
}
inline EC_word
list(int size, long* array)
{
 return EC_word(ec_listoflong(size, array));
}
inline EC_word
list(int size, char* array)
{
 return EC_word(ec_listofchar(size, array));
}
inline EC_word
array(int size, double* array)
{
 return EC_word(ec_arrayofdouble(size, array));
}
inline EC_word
matrix(int rows, int cols, double* array)
{
 return EC_word(ec_matrixofdouble(rows, cols, array));
}
inline EC_word
handle(const t_ext_type *cl, const t_ext_ptr data)
{
    return ec_handle(cl, data);
}
inline EC_word
newvar()
{
    EC_word t(ec_newvar());
    return t;
}
inline EC_word
nil()
{
    EC_word t(ec_nil());
    return t;
}
inline EC_word operator+(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.plus)),a,b); }
inline EC_word operator+(const EC_word a) { return term(EC_functor((ec_.d.plus1)),a); }
inline EC_word operator-(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.minus)),a,b); }
inline EC_word operator-(const EC_word a) { return term(EC_functor((ec_.d.minus1)),a); }
inline EC_word operator*(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.times)),a,b); }
inline EC_word operator/(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.quotient)),a,b); }
inline EC_word operator%(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.modulo)),a,b); }
inline EC_word operator>>(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.rshift)),a,b); }
inline EC_word operator<<(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.lshift)),a,b); }
inline EC_word operator&(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.and2)),a,b); }
inline EC_word operator|(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.or2)),a,b); }
inline EC_word pow(const EC_word a,const EC_word b) { return term(EC_functor((ec_.d.power)),a,b); }
inline EC_word operator~(const EC_word a) { return term(EC_functor((ec_.d.bitnot)),a); }
inline EC_word abs(const EC_word a) { return term(EC_functor((ec_.d.abs)),a); }
inline EC_word sin(const EC_word a) { return term(EC_functor((ec_.d.sin)),a); }
inline EC_word cos(const EC_word a) { return term(EC_functor((ec_.d.cos)),a); }
inline EC_word tan(const EC_word a) { return term(EC_functor((ec_.d.tan)),a); }
inline EC_word asin(const EC_word a) { return term(EC_functor((ec_.d.asin)),a); }
inline EC_word acos(const EC_word a) { return term(EC_functor((ec_.d.acos)),a); }
inline EC_word atan(const EC_word a) { return term(EC_functor((ec_.d.atan)),a); }
inline EC_word sqrt(const EC_word a) { return term(EC_functor((ec_.d.sqrt)),a); }
inline EC_word ln(const EC_word a) { return term(EC_functor((ec_.d.ln)),a); }
inline EC_word fix(const EC_word a) { return term(EC_functor((ec_.d.fix)),a); }
inline EC_word round(const EC_word a) { return term(EC_functor((ec_.d.round)),a); }
